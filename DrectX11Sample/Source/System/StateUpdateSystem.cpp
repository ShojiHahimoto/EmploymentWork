#include "System/StateUpdateSystem.h"

#include "Component/TransformComponent.h"
#include "World/World.h"

namespace
{
	constexpr int GroundAttackDurationFrames = 24;
	constexpr int AirAttackDurationFrames = 24;
	constexpr int HitstunDurationFrames = 30;
}

// Updates PlayerActionState for player objects.
// This is the only place that confirms the final action and advances actionFrame.
void StateUpdateSystem::Update(World& world)
{
	for (GameObject& object : world.GetGameObjects())
	{
		if (object.tag != GameObjectTag::Player)
		{
			continue;
		}

		UpdatePlayerState(world, object.id);
	}
}

// Checks required components, then updates one player's action state.
void StateUpdateSystem::UpdatePlayerState(World& world, GameObjectId objectId)
{
	StateComponent* state = world.GetComponent<StateComponent>(objectId);
	InputHistoryComponent* inputHistory = world.GetComponent<InputHistoryComponent>(objectId);
	if (!world.GetTransform(objectId) || !state || !inputHistory)
	{
		return;
	}

	const PlayerActionState nextActionState = DecideNextActionState(*state, *inputHistory);
	ApplyActionState(*state, nextActionState);
}

// Priority order:
// 1. hitstun request interrupts everything, including attack startup.
// 2. locked actions continue until finished or cancelable.
// 3. neutral input can choose a new action.
PlayerActionState StateUpdateSystem::DecideNextActionState(
	const StateComponent& state,
	const InputHistoryComponent& inputHistory)
{
	const InputHistoryFrame& inputFrame = inputHistory.frames[inputHistory.latestFrameIndex];

	if (state.hitstunRequested)
	{
		return PlayerActionState::Hitstun;
	}

	if (IsLockedAction(state.currentActionState)
		&& !IsActionFinished(state)
		&& !CanCancelAction(state))
	{
		return state.currentActionState;
	}

	return DecideNeutralActionState(state, inputFrame);
}

// Chooses an action when the current action can accept new input.
PlayerActionState StateUpdateSystem::DecideNeutralActionState(
	const StateComponent& state,
	const InputHistoryFrame& inputFrame)
{
	if (HasAttackTrigger(inputFrame))
	{
		return state.isGrounded ? PlayerActionState::GroundAttack : PlayerActionState::AirAttack;
	}

	if (!state.isGrounded)
	{
		return PlayerActionState::Fall;
	}

	return HasHorizontalMoveDirection(inputFrame.direction)
		? PlayerActionState::Walk
		: PlayerActionState::Idle;
}

bool StateUpdateSystem::HasHorizontalMoveDirection(int direction)
{
	return direction == 1 || direction == 3
		|| direction == 4 || direction == 6
		|| direction == 7 || direction == 9;
}

bool StateUpdateSystem::HasAttackTrigger(const InputHistoryFrame& inputFrame)
{
	return inputFrame.lightAttack.trigger
		|| inputFrame.mediumAttack.trigger
		|| inputFrame.heavyAttack.trigger;
}

bool StateUpdateSystem::IsLockedAction(PlayerActionState actionState)
{
	return actionState == PlayerActionState::GroundAttack
		|| actionState == PlayerActionState::AirAttack
		|| actionState == PlayerActionState::Hitstun;
}

bool StateUpdateSystem::IsActionFinished(const StateComponent& state)
{
	switch (state.currentActionState)
	{
	case PlayerActionState::GroundAttack:
		return state.actionFrame >= GroundAttackDurationFrames;
	case PlayerActionState::AirAttack:
		return state.actionFrame >= AirAttackDurationFrames;
	case PlayerActionState::Hitstun:
		return state.actionFrame >= HitstunDurationFrames;
	default:
		return true;
	}
}

bool StateUpdateSystem::CanCancelAction(const StateComponent& state)
{
	return state.cancelEnabled;
}

// Resets actionFrame on action changes.
// Keeps actionFrame advancing while the same action continues.
void StateUpdateSystem::ApplyActionState(StateComponent& state, PlayerActionState nextActionState)
{
	if (state.currentActionState != nextActionState)
	{
		state.currentActionState = nextActionState;
		state.actionFrame = 0;

		state.hitstunRequested = false;
		state.cancelEnabled = false;
		return;
	}

	++state.actionFrame;
	state.hitstunRequested = false;
}
