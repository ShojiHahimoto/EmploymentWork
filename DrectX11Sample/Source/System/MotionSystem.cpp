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
	constexpr const char* CommonIdleMotionDataId = "Common/Idle";
	constexpr const char* CommonDownMotionDataId = "Common/Down";
	constexpr const char* CommonAirToDownMotionDataId = "Common/AirToDown";
	constexpr int DefaultCommonBlendFrames = 3;
	constexpr int JumpLoopBlendFrames = 2;
	constexpr int DownBlendFrames = 4;
	constexpr int NoMotionBlendFrames = 0;

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
			SyncMotionPlayerFromState(world, object.id, *motionPlayer, *pose);
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
	MotionPlayerComponent& player,
	const SkeletonPoseComponent& currentPose)
{
	const StateComponent* state = world.GetComponent<StateComponent>(objectId);
	const HitBoxComponent* hitBox = world.GetComponent<HitBoxComponent>(objectId);
	const CharacterAttackDataComponent* attackData = world.GetComponent<CharacterAttackDataComponent>(objectId);
	if (!state)
	{
		return;
	}

	if (!IsAttackActionState(state->currentActionState))
	{
		const char* commonMotionDataId = GetCommonMotionDataId(*state);
		if (!commonMotionDataId || commonMotionDataId[0] == '\0')
		{
			if (player.stateDriven)
			{
				player.motionDataId.clear();
				player.currentFrame = 0;
				player.playing = false;
				player.blending = false;
				player.blendFromBonePoses.clear();
				player.boundActionState = state->currentActionState;
				player.boundAttackSlotId.clear();
			}
			return;
		}

		const bool restarted = !player.stateDriven
			|| player.motionDataId != commonMotionDataId
			|| player.boundActionState != state->currentActionState;
		if (restarted)
		{
			StartMotionBlend(player, currentPose, player.boundActionState, state->currentActionState);
		}

		player.stateDriven = true;
		player.motionDataId = commonMotionDataId;
		player.boundActionState = state->currentActionState;
		player.boundAttackSlotId.clear();
		player.looping = false;
		player.playing = true;
		player.currentFrame = restarted ? 0 : std::max(0, player.currentFrame);
		return;
	}

	if (!hitBox || !attackData || hitBox->currentAttack.slotId.empty())
	{
		if (player.stateDriven)
		{
			player.motionDataId.clear();
			player.currentFrame = 0;
			player.playing = false;
			player.blending = false;
			player.blendFromBonePoses.clear();
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
			player.blending = false;
			player.blendFromBonePoses.clear();
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
	if (restarted)
	{
		StartMotionBlend(player, currentPose, player.boundActionState, state->currentActionState);
	}

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
/// PlayerActionState に対応する汎用 MotionData ID を取得する。
/// </summary>
/// <param name="state">確認する PlayerActionState と補助状態。</param>
/// <returns>Common Motion の ID。未対応の場合は空文字。</returns>
const char* MotionSystem::GetCommonMotionDataId(const StateComponent& state)
{
	switch (state.currentActionState)
	{
	case PlayerActionState::Idle:
		return CommonIdleMotionDataId;
	case PlayerActionState::Crouch:
		return "Common/Crouch";
	case PlayerActionState::StandGuard:
	case PlayerActionState::StandGuardstun:
		return "Common/Guard";
	case PlayerActionState::CrouchGuard:
	case PlayerActionState::CrouchGuardstun:
		return "Common/CrouchGuard";
	case PlayerActionState::FrontWalk:
		return "Common/WalkForward";
	case PlayerActionState::BackWalk:
		return "Common/WalkBack";
	case PlayerActionState::VerticalJumpStartup:
	case PlayerActionState::FrontJumpStartup:
	case PlayerActionState::BackJumpStartup:
		return "Common/JumpStart";
	case PlayerActionState::VerticalJump:
	case PlayerActionState::FrontJump:
	case PlayerActionState::BackJump:
	case PlayerActionState::Fall:
		return "Common/JumpLoop";
	case PlayerActionState::AirHitstun:
		return "Common/AirHitstun";
	case PlayerActionState::LandingRecovery:
		return CommonIdleMotionDataId;
	case PlayerActionState::Hitstun:
		return "Common/Hitstun";
	case PlayerActionState::Down:
		return state.downMotionType == DownMotionType::AirToDown
			? CommonAirToDownMotionDataId
			: CommonDownMotionDataId;
	case PlayerActionState::WakeUp:
		return "Common/Wakeup";
	case PlayerActionState::GroundAttack:
	case PlayerActionState::AirAttack:
	default:
		return "";
	}
}

/// <summary>
/// モーション遷移時に何フレームかけて前フレーム姿勢から遷移先姿勢へ混ぜるかを取得する。
/// </summary>
/// <param name="previousActionState">遷移前の PlayerActionState。</param>
/// <param name="nextActionState">遷移後の PlayerActionState。</param>
/// <returns>ブレンドするフレーム数。0 の場合は即時切り替え。</returns>
int MotionSystem::GetMotionBlendFrames(PlayerActionState previousActionState, PlayerActionState nextActionState)
{
	if (previousActionState == nextActionState)
	{
		return NoMotionBlendFrames;
	}

	switch (nextActionState)
	{
	case PlayerActionState::GroundAttack:
	case PlayerActionState::AirAttack:
	case PlayerActionState::Hitstun:
	case PlayerActionState::AirHitstun:
	case PlayerActionState::VerticalJumpStartup:
	case PlayerActionState::FrontJumpStartup:
	case PlayerActionState::BackJumpStartup:
		return NoMotionBlendFrames;
	case PlayerActionState::Down:
		return DownBlendFrames;
	case PlayerActionState::VerticalJump:
	case PlayerActionState::FrontJump:
	case PlayerActionState::BackJump:
	case PlayerActionState::Fall:
		return JumpLoopBlendFrames;
	default:
		return DefaultCommonBlendFrames;
	}
}

/// <summary>
/// モーション切り替え直前に表示されていた姿勢を保存し、必要ならブレンドを開始する。
/// </summary>
/// <param name="player">ブレンド情報を書き込む MotionPlayerComponent。</param>
/// <param name="currentPose">遷移直前に SkeletonPoseComponent が保持していた表示中姿勢。</param>
/// <param name="previousActionState">遷移前の PlayerActionState。</param>
/// <param name="nextActionState">遷移後の PlayerActionState。</param>
void MotionSystem::StartMotionBlend(
	MotionPlayerComponent& player,
	const SkeletonPoseComponent& currentPose,
	PlayerActionState previousActionState,
	PlayerActionState nextActionState)
{
	const int blendFrames = player.stateDriven
		? GetMotionBlendFrames(previousActionState, nextActionState)
		: NoMotionBlendFrames;

	if (blendFrames <= 0 || currentPose.bonePoses.empty())
	{
		player.blending = false;
		player.blendFrame = 0;
		player.blendDurationFrames = 0;
		player.blendFromBonePoses.clear();
		return;
	}

	player.blending = true;
	player.blendFrame = 0;
	player.blendDurationFrames = blendFrames;
	player.blendFromBonePoses = currentPose.bonePoses;
}

/// <summary>
/// MotionData ID が攻撃モーション用の保存領域を指しているかを判定する。
/// </summary>
/// <param name="motionDataId">確認する MotionData ID。</param>
/// <returns>Attack/ から始まる攻撃モーションの場合は true。</returns>
bool MotionSystem::IsAttackMotionDataId(const std::string& motionDataId)
{
	return motionDataId.rfind("Attack/", 0) == 0;
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

	SkeletonPoseComponent idleBasePose;
	SkeletonPoseComponent transitionBasePose;
	const SkeletonPoseComponent* basePose = nullptr;
	if (IsAttackMotionDataId(player.motionDataId) && MotionDataManager::LoadMotionData(CommonIdleMotionDataId))
	{
		const MotionData* idleMotion = MotionDataManager::GetMotionData(CommonIdleMotionDataId);
		if (idleMotion)
		{
			ApplyMotionData(idleBasePose, *idleMotion, 0, model);
			basePose = &idleBasePose;
		}
	}
	else if (player.blending && player.blendFromBonePoses.size() == pose.bonePoses.size())
	{
		transitionBasePose.bonePoses = player.blendFromBonePoses;
		basePose = &transitionBasePose;
	}

	ApplyMotionData(pose, *motion, player.currentFrame, model, basePose);
	if (player.blending)
	{
		BlendBonePoses(pose, player.blendFromBonePoses, player.blendFrame, player.blendDurationFrames);
		++player.blendFrame;
		if (player.blendFrame >= player.blendDurationFrames)
		{
			player.blending = false;
			player.blendFrame = 0;
			player.blendDurationFrames = 0;
			player.blendFromBonePoses.clear();
		}
	}

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
	ApplyMotionData(pose, motion, frame, model, nullptr);
}

/// <summary>
/// 指定 MotionData を、別姿勢を下地にして SkeletonPoseComponent へ反映する。
/// </summary>
/// <param name="pose">変更する姿勢 Component。</param>
/// <param name="motion">適用するモーションデータ。</param>
/// <param name="frame">再生する 0 始まりフレーム。</param>
/// <param name="model">ボーン名検索と bind pose 取得に使う ModelResource。</param>
/// <param name="basePose">最初のキー以前や未指定ボーンに使う下地姿勢。nullptr の場合は bind pose。</param>
void MotionSystem::ApplyMotionData(
	SkeletonPoseComponent& pose,
	const MotionData& motion,
	int frame,
	const ModelResource& model,
	const SkeletonPoseComponent* basePose)
{
	ResetPoseToBindPose(pose, model);
	if (basePose && basePose->bonePoses.size() == pose.bonePoses.size())
	{
		pose.bonePoses = basePose->bonePoses;
	}

	for (const MotionBoneTrackData& track : motion.boneTracks)
	{
		const int boneIndex = FindMotionBoneIndex(model, track.boneName);
		if (boneIndex < 0 || static_cast<size_t>(boneIndex) >= pose.bonePoses.size())
		{
			continue;
		}

		const BonePose bindPose =
		{
			pose.bonePoses[boneIndex].localPosition,
			pose.bonePoses[boneIndex].localRotation,
			pose.bonePoses[boneIndex].localScale
		};

		pose.bonePoses[boneIndex] = SampleBoneTrack(track, bindPose, frame, motion.totalFrames, motion.looping);
	}
}

/// <summary>
/// 1 ボーントラックから指定フレームのローカル姿勢を補間して取得する。
/// </summary>
/// <param name="track">参照するボーンキーフレーム配列。</param>
/// <param name="bindPose">未指定チャンネルに使う bind pose。</param>
/// <param name="frame">取得する 0 始まりフレーム。</param>
/// <param name="totalFrames">ループ境界補間に使う MotionData の総フレーム。</param>
/// <param name="looping">最後のキーから最初のキーへ補間する場合は true。</param>
/// <returns>指定フレームにおける 1 ボーン分のローカル姿勢。</returns>
BonePose MotionSystem::SampleBoneTrack(
	const MotionBoneTrackData& track,
	const BonePose& bindPose,
	int frame,
	int totalFrames,
	bool looping)
{
	BonePose result = bindPose;
	if (track.keyframes.empty())
	{
		return result;
	}

	const int clampedTotalFrames = std::max(1, totalFrames);
	if (looping)
	{
		frame %= clampedTotalFrames;
		if (frame < 0)
		{
			frame += clampedTotalFrames;
		}
	}

	const MotionBoneKeyframeData* previousKey = nullptr;
	const MotionBoneKeyframeData* nextKey = nullptr;
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

	if (looping)
	{
		if (!previousKey)
		{
			previousKey = &track.keyframes.back();
		}
		if (!nextKey)
		{
			nextKey = &track.keyframes.front();
		}

		const bool crossesLoop = previousKey->frame > nextKey->frame;
		const int frameSpan = crossesLoop
			? std::max(1, (clampedTotalFrames - previousKey->frame) + nextKey->frame)
			: std::max(1, nextKey->frame - previousKey->frame);
		const int frameOffset = crossesLoop && frame < nextKey->frame
			? (clampedTotalFrames - previousKey->frame) + frame
			: frame - previousKey->frame;
		const float t = previousKey == nextKey
			? 0.0f
			: std::clamp(static_cast<float>(frameOffset) / static_cast<float>(frameSpan), 0.0f, 1.0f);

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

	if (!previousKey)
	{
		previousKey = &track.keyframes.front();
	}
	if (!nextKey)
	{
		nextKey = &track.keyframes.back();
	}

	// 最初のキーより前は、下地姿勢から最初のキーへ補間する。
	// これにより、攻撃開始直後に最初のキー姿勢へ瞬間移動せず、自然に入り始める。
	if (frame < track.keyframes.front().frame)
	{
		const MotionBoneKeyframeData& firstKey = track.keyframes.front();
		const int frameSpan = std::max(1, firstKey.frame + 1);
		const float t = std::clamp(static_cast<float>(frame + 1) / static_cast<float>(frameSpan), 0.0f, 1.0f);

		if (firstKey.hasPosition)
		{
			result.localPosition = Vector3::Lerp(bindPose.localPosition, firstKey.localPosition, t);
		}

		if (firstKey.hasRotation)
		{
			result.localRotation = Quaternion::Slerp(bindPose.localRotation, firstKey.localRotation, t);
			result.localRotation.Normalize();
		}

		if (firstKey.hasScale)
		{
			result.localScale = Vector3::Lerp(bindPose.localScale, firstKey.localScale, t);
		}

		return result;
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
/// 遷移先として計算済みの姿勢を、遷移直前姿勢から指定フレーム数で補間する。
/// </summary>
/// <param name="targetPose">遷移先姿勢が入っている SkeletonPoseComponent。ここへブレンド後姿勢を書き戻す。</param>
/// <param name="blendFromBonePoses">遷移直前に画面へ出ていたボーン姿勢配列。</param>
/// <param name="blendFrame">ブレンド開始からの経過フレーム。</param>
/// <param name="blendDurationFrames">ブレンドに使う総フレーム数。</param>
void MotionSystem::BlendBonePoses(
	SkeletonPoseComponent& targetPose,
	const std::vector<BonePose>& blendFromBonePoses,
	int blendFrame,
	int blendDurationFrames)
{
	if (blendDurationFrames <= 0 || blendFromBonePoses.size() != targetPose.bonePoses.size())
	{
		return;
	}

	const float blendRate = std::clamp(
		static_cast<float>(blendFrame + 1) / static_cast<float>(blendDurationFrames),
		0.0f,
		1.0f);

	for (size_t boneIndex = 0; boneIndex < targetPose.bonePoses.size(); ++boneIndex)
	{
		BonePose& target = targetPose.bonePoses[boneIndex];
		const BonePose& from = blendFromBonePoses[boneIndex];

		target.localPosition = Vector3::Lerp(from.localPosition, target.localPosition, blendRate);
		target.localRotation = Quaternion::Slerp(from.localRotation, target.localRotation, blendRate);
		target.localRotation.Normalize();
		target.localScale = Vector3::Lerp(from.localScale, target.localScale, blendRate);
	}
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

	if (player.looping || motion.looping)
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
