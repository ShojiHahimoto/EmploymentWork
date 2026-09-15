#pragma once

#include "Component/SkeletonPoseComponent.h"
#include "System/MotionPose.h"
#include "Component/MotionPlayerComponent.h"
#include "Core/GameObject.h"
#include "Data/AttackData.h"
#include "Data/MotionData.h"
#include "Resource/ModelResource.h"

#include <SimpleMath.h>

#include <string>

class World;
struct CharacterAttackDataComponent;
struct HitBoxComponent;
struct StateComponent;

/// <summary>
/// ModelResource のボーン情報と GameObject ごとの姿勢 Component から、描画用スキニング行列を作る。
/// </summary>
class MotionSystem
{
public:
	static void Update(World& world);

private:
	static void SyncMotionPlayerFromState(
		World& world,
		GameObjectId objectId,
		MotionPlayerComponent& player,
		const SkeletonPoseComponent& currentPose);
	static const CharacterAssignedAttackData* FindAssignedAttack(
		const CharacterAttackDataComponent* attackData,
		const std::string& attackSlotId);
	static const char* GetCommonMotionDataId(const StateComponent& state);
	static int GetMotionBlendFrames(PlayerActionState previousActionState, PlayerActionState nextActionState);
	static void StartMotionBlend(
		MotionPlayerComponent& player,
		const SkeletonPoseComponent& currentPose,
		PlayerActionState previousActionState,
		PlayerActionState nextActionState);
	static bool IsAttackMotionDataId(const std::string& motionDataId);
	static bool IsAttackActionState(PlayerActionState actionState);
	static bool ApplyMotionPlayer(SkeletonPoseComponent& pose, MotionPlayerComponent& player, const ModelResource& model);
	static DirectX::SimpleMath::Vector3 SampleRootOffset(
		const MotionData& motion,
		int frame);
	static void BlendBonePoses(
		SkeletonPoseComponent& targetPose,
		const std::vector<BonePose>& blendFromBonePoses,
		int blendFrame,
		int blendDurationFrames);
	static void AdvanceMotionFrame(MotionPlayerComponent& player, const MotionData& motion);
	static void ApplyDebugPose(SkeletonPoseComponent& pose, const ModelResource& model);
};
