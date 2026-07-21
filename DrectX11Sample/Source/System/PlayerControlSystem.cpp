#include "System/PlayerControlSystem.h"

#include "Component/TransformComponent.h"
#include "System/MovementSystem.h"
#include "World/World.h"

#include <cmath>
#include <cstddef>

using namespace DirectX::SimpleMath;

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

	const PlayerControlFrameResult result = DecideFrameResult(*state, *inputHistory);
	ApplyFrameResult(*velocity, *state, result);
}

PlayerControlFrameResult PlayerControlSystem::DecideFrameResult(
	const StateComponent& state,
	const InputHistoryComponent& inputHistory)
{
	const Input::PlayerInputState& currentInput = inputHistory.frames[inputHistory.latestFrameIndex];
	const Input::InputActionState& moveAction =
		currentInput.actions[static_cast<size_t>(Input::InputActionId::Move)];

	PlayerControlFrameResult result;
	result.horizontalInput = moveAction.value.axis.x;

	switch (state.currentState)
	{
	case ObjectState::Jump:
	case ObjectState::Fall:
		// 空中制御は未実装。既存の Y 速度を壊さないため、横方向だけここで決める。
		result.nextState = state.currentState;
		result.velocity.x = result.horizontalInput * MoveSpeedPerFrame;
		break;

	case ObjectState::Idle:
	case ObjectState::Walk:
	default:
		result.nextState = DecideGroundState(result.horizontalInput);
		result.velocity.x = result.horizontalInput * MoveSpeedPerFrame;
		break;
	}

	return result;
}

ObjectState PlayerControlSystem::DecideGroundState(float horizontalInput)
{
	return std::abs(horizontalInput) > 0.0f ? ObjectState::Walk : ObjectState::Idle;
}

void PlayerControlSystem::ApplyFrameResult(
	VelocityComponent& velocity,
	StateComponent& state,
	const PlayerControlFrameResult& result)
{
	MovementSystem::SetVelocityX(velocity, result.velocity.x);

	if (state.currentState != result.nextState)
	{
		state.currentState = result.nextState;
		state.stateFrame = 0;
		return;
	}

	++state.stateFrame;
}
