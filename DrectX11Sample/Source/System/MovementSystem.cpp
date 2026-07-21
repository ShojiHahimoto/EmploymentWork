#include "System/MovementSystem.h"

#include "Component/TransformComponent.h"
#include "Component/VelocityComponent.h"
#include "Input/InputSystem.h"
#include "System/TransformSystem.h"
#include "World/World.h"

using namespace DirectX::SimpleMath;

void MovementSystem::Update(World& world)
{
	UpdateVelocityFromInput(world);
	ApplyVelocityToTransform(world);
}

void MovementSystem::UpdateVelocityFromInput(World& world)
{
	const Input::InputActionState& moveAction =
		Input::InputSystem::GetActionState(0, Input::InputActionId::Move);

	const float horizontalInput = moveAction.value.axis.x;

	for (GameObject& object : world.GetGameObjects())
	{
		VelocityComponent* velocity = world.GetComponent<VelocityComponent>(object.id);
		if (!velocity)
		{
			continue;
		}

		// 今回は入力確認用なので、横方向は慣性を持たせず毎フレーム上書きする。
		velocity->velocity.x = horizontalInput * MoveSpeedPerFrame;
	}
}

void MovementSystem::ApplyVelocityToTransform(World& world)
{
	for (GameObject& object : world.GetGameObjects())
	{
		TransformComponent* transform = world.GetTransform(object.id);
		VelocityComponent* velocity = world.GetComponent<VelocityComponent>(object.id);
		if (!transform || !velocity)
		{
			continue;
		}

		// Velocity は 1 フレームあたりの移動量として扱い、localPosition へ即時反映する。
		TransformSystem::SetLocalPosition(
			*transform,
			TransformSystem::GetLocalPosition(*transform) + velocity->velocity);
	}
}
