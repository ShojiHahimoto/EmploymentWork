#pragma once

#include "Component/InputHistoryComponent.h"
#include "Component/StateComponent.h"
#include "Core/GameObject.h"

#include <string>

class World;
struct CharacterAttackDataComponent;
struct HitBoxComponent;
struct VelocityComponent;
struct TransformComponent;

struct PlayerActionDecision
{
	PlayerActionState nextActionState = PlayerActionState::Idle;

	// Same PlayerActionState can be restarted by a new accepted input.
	// Example: a finished attack accepts another attack and actionFrame must return to 0.
	bool restartAction = false;

	// 攻撃へ遷移する場合に、CharacterAttackDataComponent のどの slotId を使うかを指定する。
	std::string attackSlotId;
};

class StateUpdateSystem
{
public:
	// Updates PlayerActionState and actionFrame from input history and current player conditions.
	static void Update(World& world);

private:
	static void UpdatePlayerState(World& world, GameObjectId objectId);
	static PlayerActionDecision DecideNextAction(
		const StateComponent& state,
		const VelocityComponent& velocity,
		const InputHistoryFrame& inputFrame,
		const CharacterAttackDataComponent* attackData,
		const HitBoxComponent* hitBox);
	static PlayerActionDecision DecideNeutralAction(const StateComponent& state, const VelocityComponent& velocity, const InputHistoryFrame& inputFrame);
	static bool HasHorizontalMoveDirection(int direction);
	static bool HasAttackTrigger(const InputHistoryFrame& inputFrame);
	static std::string SelectAttackSlotId(const InputHistoryFrame& inputFrame);
	static bool IsLockedAction(PlayerActionState actionState);
	static bool IsActionFinished(const StateComponent& state, const CharacterAttackDataComponent* attackData, const HitBoxComponent* hitBox);
	static int GetCurrentAttackTotalFrames(const CharacterAttackDataComponent* attackData, const HitBoxComponent* hitBox);
	static bool CanCancelAction(const StateComponent& state);
	static void ApplyActionState(StateComponent& state, HitBoxComponent* hitBox, const PlayerActionDecision& decision);
	static void ApplyPlayerDirection(World& world, GameObjectId objectId, StateComponent& state, const TransformComponent& transform);
};
