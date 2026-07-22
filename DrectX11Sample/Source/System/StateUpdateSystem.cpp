#include "System/StateUpdateSystem.h"

#include "Component/TransformComponent.h"
#include "Component/VelocityComponent.h"
#include "World/World.h"

namespace
{
	constexpr int GroundAttackDurationFrames = 24;
	constexpr int AirAttackDurationFrames = 24;
	constexpr int HitstunDurationFrames = 30;
}

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

void StateUpdateSystem::UpdatePlayerState(World& world, GameObjectId objectId)
{
	StateComponent* state = world.GetComponent<StateComponent>(objectId);
	VelocityComponent* velocity = world.GetComponent<VelocityComponent>(objectId);
	InputHistoryComponent* inputHistory = world.GetComponent<InputHistoryComponent>(objectId);
	if (!world.GetTransform(objectId) || !state || !velocity || !inputHistory)
	{
		return;
	}

	// Count this frame first. If the action changes below, ApplyActionState resets it to 0.
	++state->actionFrame;

	const PlayerActionDecision decision = DecideNextAction(*state, *velocity, *inputHistory);
	ApplyActionState(*state, decision);
}

PlayerActionDecision StateUpdateSystem::DecideNextAction(
	const StateComponent& state,
	const VelocityComponent& velocity,
	const InputHistoryComponent& inputHistory)
{
	const InputHistoryFrame& inputFrame = inputHistory.frames[inputHistory.latestFrameIndex];

	if (state.hitstunRequested)
	{
		return { PlayerActionState::Hitstun, true };
	}

	if (IsLockedAction(state.currentActionState)
		&& !IsActionFinished(state)
		&& !CanCancelAction(state))
	{
		return { state.currentActionState, false };
	}

	return DecideNeutralAction(state, velocity, inputFrame);
}

PlayerActionDecision StateUpdateSystem::DecideNeutralAction(
	const StateComponent& state,
	const VelocityComponent& velocity,
	const InputHistoryFrame& inputFrame)
{
	if (HasAttackTrigger(inputFrame))
	{
		return {
			state.isGrounded ? PlayerActionState::GroundAttack : PlayerActionState::AirAttack,
			true
		};
	}

	if (state.isGrounded && inputFrame.jump.trigger)
	{
		return { PlayerActionState::Jump, true };
	}

	if (!state.isGrounded)
	{
		return {
			velocity.velocity.y > 0.0f ? PlayerActionState::Jump : PlayerActionState::Fall,
			false
		};
	}

	return {
		HasHorizontalMoveDirection(inputFrame.direction) ? PlayerActionState::Walk : PlayerActionState::Idle,
		false
	};
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

void StateUpdateSystem::ApplyActionState(StateComponent& state, const PlayerActionDecision& decision)
{
	if (state.currentActionState != decision.nextActionState || decision.restartAction)
	{
		state.currentActionState = decision.nextActionState;
		state.actionFrame = 0;
		state.cancelEnabled = false;
	}

	state.hitstunRequested = false;
}
