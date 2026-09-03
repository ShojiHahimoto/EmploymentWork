#include "System/MotionSystem.h"

#include "Component/ModelComponent.h"
#include "World/World.h"

#include <DirectXMath.h>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace DirectX;
using namespace DirectX::SimpleMath;

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
