#pragma once

#include "Component/SkeletonPoseComponent.h"
#include "Customize/MotionPose.h"
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
	/// <summary>現在フレームの姿勢を反映し、攻撃では地上・空中別の基準姿勢へ前後を補間する。</summary>
	/// <param name="pose">反映先の姿勢。</param>
	/// <param name="player">再生対象と現在フレーム。</param>
	/// <param name="model">対象モデル。</param>
	/// <param name="attack">実行中の技。通常モーションではnullptr。</param>
	/// <returns>モーションを取得して姿勢へ反映できた場合true。</returns>
	static bool ApplyMotionPlayer(SkeletonPoseComponent& pose, MotionPlayerComponent& player,
		const ModelResource& model, const AttackData* attack);
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
