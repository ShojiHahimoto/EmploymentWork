#pragma once

#include "Data/SkeletonPose.h"
#include "Data/MotionData.h"
#include "Data/AttackData.h"
#include "Resource/ModelResource.h"

/// <summary>
/// 編集と対戦で共有する姿勢計算。状態遷移や再生時計は変更しない。
/// </summary>
class MotionPose
{
public:
	static bool GetBoneWorldMatrix(const SkeletonPose& pose, int boneIndex,
		const DirectX::SimpleMath::Matrix& objectWorld, DirectX::SimpleMath::Matrix& worldMatrix);

	static bool WorldToLocalPose(const SkeletonPose& pose, const ModelResource& model, int boneIndex,
		const DirectX::SimpleMath::Matrix& objectWorld, const DirectX::SimpleMath::Matrix& worldMatrix,
		BonePose& localPose);

	static bool InitializeSkeletonPose(
		SkeletonPose& pose,
		const ModelResource& model,
		const std::string& modelKey);

	static bool SetBoneLocalEulerRotationDegrees(
		SkeletonPose& pose,
		const ModelResource& model,
		const std::string& boneName,
		const DirectX::SimpleMath::Vector3& eulerDegrees);

	static void UpdateSkinningMatrices(SkeletonPose& pose, const ModelResource& model);

	static void ApplyMotionData(
		SkeletonPose& pose,
		const MotionData& motion,
		int frame,
		const ModelResource& model);

	static void ApplyMotionData(
		SkeletonPose& pose,
		const MotionData& motion,
		int frame,
		const ModelResource& model,
		const SkeletonPose* basePose, bool returnToBase = false);

	static const char* GetAttackBaseMotionId(AttackUsableState usableState);

	static void ResetPoseToBindPose(SkeletonPose& pose, const ModelResource& model);

	static BonePose SampleBoneTrack(
		const MotionBoneTrackData& track,
		const BonePose& bindPose,
		int frame,
		int totalFrames,
		bool looping, bool returnToBase = false);

private:
	static DirectX::SimpleMath::Matrix CreateLocalMatrix(const BonePose& pose);
};
