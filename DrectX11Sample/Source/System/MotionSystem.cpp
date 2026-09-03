#include "System/MotionSystem.h"

#include "Component/CharacterAttackDataComponent.h"
#include "Component/HitBoxComponent.h"
#include "Component/ModelComponent.h"
#include "Component/MotionPlayerComponent.h"
#include "Component/StateComponent.h"
#include "Data/MotionData.h"
#include "World/World.h"

#include <DirectXMath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

using namespace DirectX;
using namespace DirectX::SimpleMath;

namespace
{
	struct MotionEditorBoneAlias
	{
		const char* editorName;
		std::array<const char*, 3> modelBoneNames;
	};

	constexpr MotionEditorBoneAlias MotionEditorBoneAliases[] =
	{
		{ "Head", { "mixamorig:Head", "Head", "" } },
		{ "Spine", { "mixamorig:Spine", "mixamorig:Spine1", "Spine" } },
		{ "Waist", { "mixamorig:Hips", "Hips", "" } },
		{ "RShoulder", { "mixamorig:RightArm", "RightArm", "mixamorig:RightShoulder" } },
		{ "LShoulder", { "mixamorig:LeftArm", "LeftArm", "mixamorig:LeftShoulder" } },
		{ "RElbow", { "mixamorig:RightForeArm", "RightForeArm", "" } },
		{ "LElbow", { "mixamorig:LeftForeArm", "LeftForeArm", "" } },
		{ "RHand", { "mixamorig:RightHand", "RightHand", "" } },
		{ "LHand", { "mixamorig:LeftHand", "LeftHand", "" } },
		{ "RHipjoint", { "mixamorig:RightUpLeg", "RightUpLeg", "" } },
		{ "LHipjoint", { "mixamorig:LeftUpLeg", "LeftUpLeg", "" } },
		{ "RKnees", { "mixamorig:RightLeg", "RightLeg", "" } },
		{ "LKnees", { "mixamorig:LeftLeg", "LeftLeg", "" } },
		{ "RFeet", { "mixamorig:RightFoot", "RightFoot", "" } },
		{ "LFeet", { "mixamorig:LeftFoot", "LeftFoot", "" } },
	};

	/// <summary>
	/// MotionData 上の編集用部位名または実ボーン名を、ModelResource 内のボーン番号へ解決する。
	/// </summary>
	/// <param name="model">検索対象の ModelResource。</param>
	/// <param name="motionBoneName">MotionData に保存されている部位名または実ボーン名。</param>
	/// <returns>見つかったボーン番号。存在しない場合は -1。</returns>
	int FindMotionBoneIndex(const ModelResource& model, const std::string& motionBoneName)
	{
		const int directBoneIndex = model.FindBoneIndex(motionBoneName);
		if (directBoneIndex >= 0)
		{
			return directBoneIndex;
		}

		for (const MotionEditorBoneAlias& alias : MotionEditorBoneAliases)
		{
			if (motionBoneName != alias.editorName)
			{
				continue;
			}

			for (const char* modelBoneName : alias.modelBoneNames)
			{
				if (!modelBoneName || modelBoneName[0] == '\0')
				{
					continue;
				}

				const int aliasedBoneIndex = model.FindBoneIndex(modelBoneName);
				if (aliasedBoneIndex >= 0)
				{
					return aliasedBoneIndex;
				}
			}
		}

		return -1;
	}
}

/// <summary>
/// モデル付き GameObject の姿勢 Component を初期化し、毎フレーム描画用スキニング行列へ更新する。
/// </summary>
/// <param name="world">ModelComponent と SkeletonPoseComponent を検索する World。</param>
void MotionSystem::Update(World& world)
{
	for (GameObject& object : world.GetGameObjects())
	{
		const ModelComponent* modelComponent = world.GetComponent<ModelComponent>(object.id);
		SkeletonPoseComponent* pose = world.GetComponent<SkeletonPoseComponent>(object.id);
		if (!modelComponent || !pose)
		{
			continue;
		}

		const ModelResource* model = ModelResourceManager::GetModel(modelComponent->resourceKey);
		if (!model)
		{
			continue;
		}

		if (!pose->initialized || pose->sourceModelKey != modelComponent->resourceKey)
		{
			InitializeSkeletonPose(*pose, *model, modelComponent->resourceKey);
		}

		MotionPlayerComponent* motionPlayer = world.GetComponent<MotionPlayerComponent>(object.id);
		if (motionPlayer)
		{
			SyncMotionPlayerFromState(world, object.id, *motionPlayer);
		}

		if (motionPlayer && ApplyMotionPlayer(*pose, *motionPlayer, *model))
		{
			UpdateSkinningMatrices(*pose, *model);
			continue;
		}

#if defined(_DEBUG)
		if (pose->enableDebugPose)
		{
			ApplyDebugPose(*pose, *model);
		}
#endif

		UpdateSkinningMatrices(*pose, *model);
	}
}

/// <summary>
/// ModelResource の bind pose を GameObject ごとの現在姿勢へコピーする。
/// </summary>
/// <param name="pose">初期化する姿勢 Component。</param>
/// <param name="model">初期ボーン姿勢を持つ ModelResource。</param>
/// <param name="modelKey">初期化元として記録するモデルキー。</param>
/// <returns>ボーンが存在し、初期化できた場合は true。</returns>
bool MotionSystem::InitializeSkeletonPose(
	SkeletonPoseComponent& pose,
	const ModelResource& model,
	const std::string& modelKey)
{
	const std::vector<ModelBone>& bones = model.GetBones();
	if (bones.empty())
	{
		pose.sourceModelKey.clear();
		pose.bonePoses.clear();
		pose.boneWorldMatrices.clear();
		pose.skinningMatrices.clear();
		pose.initialized = false;
		return false;
	}

	pose.sourceModelKey = modelKey;
	pose.bonePoses.resize(bones.size());
	pose.boneWorldMatrices.assign(bones.size(), Matrix::Identity);
	pose.skinningMatrices.assign(bones.size(), Matrix::Identity);
	ResetPoseToBindPose(pose, model);
	UpdateSkinningMatrices(pose, model);
	pose.initialized = true;

	return true;
}

/// <summary>
/// 現在の PlayerActionState と実行中攻撃スロットから MotionPlayerComponent の再生対象を更新する。
/// </summary>
/// <param name="world">State / HitBox / CharacterAttackData を取得する World。</param>
/// <param name="objectId">同期対象の GameObject ID。</param>
/// <param name="player">更新する MotionPlayerComponent。</param>
void MotionSystem::SyncMotionPlayerFromState(
	World& world,
	GameObjectId objectId,
	MotionPlayerComponent& player)
{
	const StateComponent* state = world.GetComponent<StateComponent>(objectId);
	const HitBoxComponent* hitBox = world.GetComponent<HitBoxComponent>(objectId);
	const CharacterAttackDataComponent* attackData = world.GetComponent<CharacterAttackDataComponent>(objectId);
	if (!state)
	{
		return;
	}

	if (!IsAttackActionState(state->currentActionState) || !hitBox || !attackData || hitBox->currentAttack.slotId.empty())
	{
		if (player.stateDriven)
		{
			player.motionDataId.clear();
			player.currentFrame = 0;
			player.playing = false;
			player.boundActionState = state->currentActionState;
			player.boundAttackSlotId.clear();
		}
		return;
	}

	const CharacterAssignedAttackData* assignedAttack = FindAssignedAttack(attackData, hitBox->currentAttack.slotId);
	if (!assignedAttack || assignedAttack->attack.motionDataId.empty())
	{
		if (player.stateDriven)
		{
			player.motionDataId.clear();
			player.currentFrame = 0;
			player.playing = false;
			player.boundActionState = state->currentActionState;
			player.boundAttackSlotId = hitBox->currentAttack.slotId;
		}
		return;
	}

	const bool restarted = !player.stateDriven
		|| player.motionDataId != assignedAttack->attack.motionDataId
		|| player.boundActionState != state->currentActionState
		|| player.boundAttackSlotId != hitBox->currentAttack.slotId
		|| state->actionFrame == 0;

	player.stateDriven = true;
	player.motionDataId = assignedAttack->attack.motionDataId;
	player.boundActionState = state->currentActionState;
	player.boundAttackSlotId = hitBox->currentAttack.slotId;
	player.looping = false;
	player.playing = true;
	player.currentFrame = restarted ? 0 : std::max(0, state->actionFrame);
}

/// <summary>
/// CharacterAttackDataComponent から指定 slotId の技データを探す。
/// </summary>
/// <param name="attackData">検索対象の CharacterAttackDataComponent。</param>
/// <param name="attackSlotId">検索する攻撃スロット ID。</param>
/// <returns>見つかった割り当て技。存在しない場合は nullptr。</returns>
const CharacterAssignedAttackData* MotionSystem::FindAssignedAttack(
	const CharacterAttackDataComponent* attackData,
	const std::string& attackSlotId)
{
	if (!attackData || attackSlotId.empty())
	{
		return nullptr;
	}

	for (const CharacterAssignedAttackData& assignedAttack : attackData->attacks)
	{
		if (assignedAttack.slotId == attackSlotId)
		{
			return &assignedAttack;
		}
	}

	return nullptr;
}

/// <summary>
/// 指定 ActionState が攻撃中か確認する。
/// </summary>
/// <param name="actionState">判定する PlayerActionState。</param>
/// <returns>地上攻撃または空中攻撃なら true。</returns>
bool MotionSystem::IsAttackActionState(PlayerActionState actionState)
{
	return actionState == PlayerActionState::GroundAttack
		|| actionState == PlayerActionState::AirAttack;
}

/// <summary>
/// モーション編集やデバッグ確認用に、指定ボーンのローカル回転を直接設定する。
/// </summary>
/// <param name="pose">変更する姿勢 Component。</param>
/// <param name="model">ボーン名検索に使う ModelResource。</param>
/// <param name="boneName">対象ボーン名。</param>
/// <param name="eulerDegrees">degree 単位のローカル Euler 回転。</param>
/// <returns>対象ボーンが見つかり、姿勢を変更できた場合は true。</returns>
bool MotionSystem::SetBoneLocalEulerRotationDegrees(
	SkeletonPoseComponent& pose,
	const ModelResource& model,
	const std::string& boneName,
	const Vector3& eulerDegrees)
{
	const int boneIndex = model.FindBoneIndex(boneName);
	if (boneIndex < 0 || static_cast<size_t>(boneIndex) >= pose.bonePoses.size())
	{
		return false;
	}

	const float pitch = XMConvertToRadians(eulerDegrees.x);
	const float yaw = XMConvertToRadians(eulerDegrees.y);
	const float roll = XMConvertToRadians(eulerDegrees.z);
	pose.bonePoses[boneIndex].localRotation = Quaternion::CreateFromYawPitchRoll(yaw, pitch, roll);
	pose.bonePoses[boneIndex].localRotation.Normalize();

	return true;
}

/// <summary>
/// 現在ローカル姿勢から親子階層を反映した行列を作り、Renderer 用のスキニング行列を更新する。
/// </summary>
/// <param name="pose">計算結果を書き込む姿勢 Component。</param>
/// <param name="model">親子階層と offsetMatrix を持つ ModelResource。</param>
void MotionSystem::UpdateSkinningMatrices(SkeletonPoseComponent& pose, const ModelResource& model)
{
	const std::vector<ModelBone>& bones = model.GetBones();
	if (bones.empty() || pose.bonePoses.size() != bones.size())
	{
		return;
	}

	pose.boneWorldMatrices.assign(bones.size(), Matrix::Identity);
	pose.skinningMatrices.assign(bones.size(), Matrix::Identity);

	for (size_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex)
	{
		const Matrix localMatrix = CreateLocalMatrix(pose.bonePoses[boneIndex]);
		const int parentIndex = bones[boneIndex].parentIndex;

		if (parentIndex >= 0 && static_cast<size_t>(parentIndex) < pose.boneWorldMatrices.size())
		{
			pose.boneWorldMatrices[boneIndex] = localMatrix * pose.boneWorldMatrices[parentIndex];
		}
		else
		{
			pose.boneWorldMatrices[boneIndex] = localMatrix;
		}

		// offsetMatrix は bind pose の逆変換、boneWorldMatrices は現在姿勢。
		// 頂点は bind 空間 -> bone 空間 -> 現在姿勢の順に変換される。
		pose.skinningMatrices[boneIndex] = bones[boneIndex].offsetMatrix * pose.boneWorldMatrices[boneIndex];
	}
}

/// <summary>
/// 現在姿勢をモデル読み込み時の bind pose へ戻す。
/// </summary>
/// <param name="pose">リセットする姿勢 Component。</param>
/// <param name="model">bind pose を持つ ModelResource。</param>
void MotionSystem::ResetPoseToBindPose(SkeletonPoseComponent& pose, const ModelResource& model)
{
	const std::vector<ModelBone>& bones = model.GetBones();
	pose.bonePoses.resize(bones.size());

	for (size_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex)
	{
		pose.bonePoses[boneIndex].localPosition = bones[boneIndex].bindLocalPosition;
		pose.bonePoses[boneIndex].localRotation = bones[boneIndex].bindLocalRotation;
		pose.bonePoses[boneIndex].localScale = bones[boneIndex].bindLocalScale;
	}
}

/// <summary>
/// MotionPlayerComponent が指定する MotionData を読み込み、現在フレームの姿勢を反映する。
/// </summary>
/// <param name="pose">変更する姿勢 Component。</param>
/// <param name="player">再生中モーション ID と現在フレームを持つ Component。</param>
/// <param name="model">ボーン名検索と bind pose 取得に使う ModelResource。</param>
/// <returns>MotionData を姿勢へ反映できた場合は true。</returns>
bool MotionSystem::ApplyMotionPlayer(
	SkeletonPoseComponent& pose,
	MotionPlayerComponent& player,
	const ModelResource& model)
{
	if (!player.playing || player.motionDataId.empty())
	{
		return false;
	}

	if (!MotionDataManager::LoadMotionData(player.motionDataId))
	{
		return false;
	}

	const MotionData* motion = MotionDataManager::GetMotionData(player.motionDataId);
	if (!motion)
	{
		return false;
	}

	ApplyMotionData(pose, *motion, player.currentFrame, model);
	AdvanceMotionFrame(player, *motion);
	return true;
}

/// <summary>
/// 指定 MotionData の指定フレームを SkeletonPoseComponent に反映する。
/// </summary>
/// <param name="pose">変更する姿勢 Component。</param>
/// <param name="motion">適用するモーションデータ。</param>
/// <param name="frame">再生する 0 始まりフレーム。</param>
/// <param name="model">ボーン名検索と bind pose 取得に使う ModelResource。</param>
void MotionSystem::ApplyMotionData(
	SkeletonPoseComponent& pose,
	const MotionData& motion,
	int frame,
	const ModelResource& model)
{
	ResetPoseToBindPose(pose, model);

	const std::vector<ModelBone>& bones = model.GetBones();
	for (const MotionBoneTrackData& track : motion.boneTracks)
	{
		const int boneIndex = FindMotionBoneIndex(model, track.boneName);
		if (boneIndex < 0 || static_cast<size_t>(boneIndex) >= pose.bonePoses.size())
		{
			continue;
		}

		const BonePose bindPose =
		{
			bones[boneIndex].bindLocalPosition,
			bones[boneIndex].bindLocalRotation,
			bones[boneIndex].bindLocalScale
		};

		pose.bonePoses[boneIndex] = SampleBoneTrack(track, bindPose, frame);
	}
}

/// <summary>
/// 1 ボーントラックから指定フレームのローカル姿勢を補間して取得する。
/// </summary>
/// <param name="track">参照するボーンキーフレーム配列。</param>
/// <param name="bindPose">未指定チャンネルに使う bind pose。</param>
/// <param name="frame">取得する 0 始まりフレーム。</param>
/// <returns>指定フレームにおける 1 ボーン分のローカル姿勢。</returns>
BonePose MotionSystem::SampleBoneTrack(
	const MotionBoneTrackData& track,
	const BonePose& bindPose,
	int frame)
{
	BonePose result = bindPose;
	if (track.keyframes.empty())
	{
		return result;
	}

	const MotionBoneKeyframeData* previousKey = &track.keyframes.front();
	const MotionBoneKeyframeData* nextKey = &track.keyframes.back();
	for (const MotionBoneKeyframeData& keyframe : track.keyframes)
	{
		if (keyframe.frame <= frame)
		{
			previousKey = &keyframe;
		}

		if (keyframe.frame >= frame)
		{
			nextKey = &keyframe;
			break;
		}
	}

	const int frameSpan = std::max(1, nextKey->frame - previousKey->frame);
	const float t = std::clamp(static_cast<float>(frame - previousKey->frame) / static_cast<float>(frameSpan), 0.0f, 1.0f);

	if (previousKey->hasPosition && nextKey->hasPosition)
	{
		result.localPosition = Vector3::Lerp(previousKey->localPosition, nextKey->localPosition, t);
	}
	else if (previousKey->hasPosition)
	{
		result.localPosition = previousKey->localPosition;
	}

	if (previousKey->hasRotation && nextKey->hasRotation)
	{
		result.localRotation = Quaternion::Slerp(previousKey->localRotation, nextKey->localRotation, t);
		result.localRotation.Normalize();
	}
	else if (previousKey->hasRotation)
	{
		result.localRotation = previousKey->localRotation;
	}

	if (previousKey->hasScale && nextKey->hasScale)
	{
		result.localScale = Vector3::Lerp(previousKey->localScale, nextKey->localScale, t);
	}
	else if (previousKey->hasScale)
	{
		result.localScale = previousKey->localScale;
	}

	return result;
}

/// <summary>
/// MotionData の総フレームとループ設定を見て、次フレームの再生位置へ進める。
/// </summary>
/// <param name="player">再生フレームを更新する MotionPlayerComponent。</param>
/// <param name="motion">総フレームとループ設定を持つ MotionData。</param>
void MotionSystem::AdvanceMotionFrame(MotionPlayerComponent& player, const MotionData& motion)
{
	const int totalFrames = std::max(1, motion.totalFrames);
	player.currentFrame += 1;

	if (player.currentFrame < totalFrames)
	{
		return;
	}

	if (!player.stateDriven && (player.looping || motion.looping))
	{
		player.currentFrame = 0;
	}
	else
	{
		player.currentFrame = totalFrames - 1;
		player.playing = false;
	}
}

/// <summary>
/// GPU スキニング確認用に、右腕系ボーンへ小さなフレームベース回転を与える。
/// </summary>
/// <param name="pose">変更する姿勢 Component。</param>
/// <param name="model">右腕ボーン名を検索する ModelResource。</param>
void MotionSystem::ApplyDebugPose(SkeletonPoseComponent& pose, const ModelResource& model)
{
	ResetPoseToBindPose(pose, model);

	const std::vector<std::string> rightArmCandidates =
	{
		"mixamorig:RightArm",
		"RightArm",
		"mixamorig:RightForeArm",
		"RightForeArm",
	};

	int targetBoneIndex = -1;
	for (const std::string& boneName : rightArmCandidates)
	{
		targetBoneIndex = model.FindBoneIndex(boneName);
		if (targetBoneIndex >= 0)
		{
			break;
		}
	}

	if (targetBoneIndex < 0 || static_cast<size_t>(targetBoneIndex) >= pose.bonePoses.size())
	{
		return;
	}

	pose.debugPoseFrame = (pose.debugPoseFrame + 1) % 360;

	const float angleDegrees = std::sin(static_cast<float>(pose.debugPoseFrame) * 0.08f) * 35.0f;
	const Quaternion addRotation = Quaternion::CreateFromYawPitchRoll(
		0.0f,
		0.0f,
		XMConvertToRadians(angleDegrees));

	// bind pose から毎フレーム作り直すことで、仮ポーズの回転が累積して破綻しないようにする。
	BonePose& bonePose = pose.bonePoses[targetBoneIndex];
	bonePose.localRotation = bonePose.localRotation * addRotation;
	bonePose.localRotation.Normalize();
}

/// <summary>
/// 1 ボーン分のローカル姿勢を行列へ変換する。
/// </summary>
/// <param name="pose">行列化するローカル姿勢。</param>
/// <returns>Scale、Rotation、Translation を合成したローカル行列。</returns>
Matrix MotionSystem::CreateLocalMatrix(const BonePose& pose)
{
	return Matrix::CreateScale(pose.localScale)
		* Matrix::CreateFromQuaternion(pose.localRotation)
		* Matrix::CreateTranslation(pose.localPosition);
}
