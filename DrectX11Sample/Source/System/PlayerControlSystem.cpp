#include "System/PlayerControlSystem.h"

#include "Component/TransformComponent.h"
#include "System/MovementSystem.h"
#include "World/World.h"

// Runs action behavior for player objects after StateUpdateSystem has confirmed PlayerActionState.
void PlayerControlSystem::Update(World& world)
{
	for (GameObject& object : world.GetGameObjects())
	{
		if (object.tag != GameObjectTag::Player)
		{
			continue;
		}

		UpdatePlayer(world, object.id);
	}
}

void PlayerControlSystem::UpdatePlayer(World& world, GameObjectId objectId)
{
	VelocityComponent* velocity = world.GetComponent<VelocityComponent>(objectId);
	StateComponent* state = world.GetComponent<StateComponent>(objectId);
	InputHistoryComponent* inputHistory = world.GetComponent<InputHistoryComponent>(objectId);
	if (!world.GetTransform(objectId) || !velocity || !state || !inputHistory)
	{
		return;
	}

	const PlayerControlFrameResult result = ExecuteCurrentAction(*state, *inputHistory);
	ApplyFrameResult(*velocity, result);
}

// Converts the confirmed action into behavior output.
// Attack and hitstun behavior are placeholders until their dedicated systems exist.
PlayerControlFrameResult PlayerControlSystem::ExecuteCurrentAction(
	const StateComponent& state,
	const InputHistoryComponent& inputHistory)
{
	PlayerControlFrameResult result;
	const InputHistoryFrame& inputFrame = inputHistory.frames[inputHistory.latestFrameIndex];

	switch (state.currentActionState)
	{
	case PlayerActionState::Walk:
		result.horizontalVelocity = GetHorizontalInputFromDirection(inputFrame.direction) * MoveSpeedPerFrame;
		break;

	case PlayerActionState::Idle:
	case PlayerActionState::GroundAttack:
	case PlayerActionState::AirAttack:
	case PlayerActionState::Hitstun:
		result.horizontalVelocity = 0.0f;
		break;

	case PlayerActionState::Jump:
	case PlayerActionState::Fall:
	default:
		break;
	}

	return result;
}

float PlayerControlSystem::GetHorizontalInputFromDirection(int direction)
{
	if (direction == 1 || direction == 4 || direction == 7)
	{
		return -1.0f;
	}

	if (direction == 3 || direction == 6 || direction == 9)
	{
		return 1.0f;
	}

	return 0.0f;
}

void PlayerControlSystem::ApplyFrameResult(
	VelocityComponent& velocity,
	const PlayerControlFrameResult& result)
{
	MovementSystem::SetVelocityX(velocity, result.horizontalVelocity);
}
