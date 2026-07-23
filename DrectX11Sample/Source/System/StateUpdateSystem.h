#pragma once

#include "Component/InputHistoryComponent.h"
#include "Component/StateComponent.h"
#include "Core/GameObject.h"

class World;
struct VelocityComponent;
struct TransformComponent;

struct PlayerActionDecision
{
	PlayerActionState nextActionState = PlayerActionState::Idle;

	// Same PlayerActionState can be restarted by a new accepted input.
	// Example: a finished attack accepts another attack and actionFrame must return to 0.
	bool restartAction = false;
};

class StateUpdateSystem
{
public:
	// Updates PlayerActionState and actionFrame from input history and current player conditions.
	static void Update(World& world);

private:
	static void UpdatePlayerState(World& world, GameObjectId objectId);
	static PlayerActionDecision DecideNextAction(const StateComponent& state, const VelocityComponent& velocity, const InputHistoryComponent& inputHistory);
	static PlayerActionDecision DecideNeutralAction(const StateComponent& state, const VelocityComponent& velocity, const InputHistoryFrame& inputFrame);
	static bool HasHorizontalMoveDirection(int direction);
	static bool HasAttackTrigger(const InputHistoryFrame& inputFrame);
	static bool IsLockedAction(PlayerActionState actionState);
	static bool IsActionFinished(const StateComponent& state);
	static bool CanCancelAction(const StateComponent& state);
	static void ApplyActionState(StateComponent& state, const PlayerActionDecision& decision);
	static void ApplyPlayerDirection(StateComponent& state,const TransformComponent& transform);
};
