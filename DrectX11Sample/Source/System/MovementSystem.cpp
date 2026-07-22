#include "System/MovementSystem.h"

#include "Component/StateComponent.h"
#include "Component/TransformComponent.h"
#include "Component/VelocityComponent.h"
#include "System/TransformSystem.h"
#include "World/World.h"

using namespace DirectX::SimpleMath;

void MovementSystem::Update(World& world)
{
	ApplyAirGravity(world);
	ApplyVelocityToTransform(world);
	ResolveTemporaryGround(world);
}

void MovementSystem::SetVelocity(VelocityComponent& velocity, const Vector3& value)
{
	velocity.velocity = value;
}

void MovementSystem::SetVelocityX(VelocityComponent& velocity, float value)
{
	velocity.velocity.x = value;
}

void MovementSystem::SetVelocityY(VelocityComponent& velocity, float value)
{
	velocity.velocity.y = value;
}

void MovementSystem::SetVelocityZ(VelocityComponent& velocity, float value)
{
	velocity.velocity.z = value;
}

void MovementSystem::AddVelocity(VelocityComponent& velocity, const Vector3& value)
{
	velocity.velocity += value;
}

void MovementSystem::AddVelocityY(VelocityComponent& velocity, float value)
{
	velocity.velocity.y += value;
}

void MovementSystem::ApplyAirGravity(World& world)
{
	for (GameObject& object : world.GetGameObjects())
	{
		TransformComponent* transform = world.GetTransform(object.id);
		StateComponent* state = world.GetComponent<StateComponent>(object.id);
		VelocityComponent* velocity = world.GetComponent<VelocityComponent>(object.id);
		if (!transform || !state || !velocity)
		{
			continue;
		}

		if (!ShouldApplyGravity(*transform, *state))
		{
			continue;
		}

		state->isGrounded = false;
		const float gravity = velocity->velocity.y > 0.0f
			? RiseGravityPerFrame
			: FallGravityPerFrame;
		AddVelocityY(*velocity, gravity);
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

void MovementSystem::ResolveTemporaryGround(World& world)
{
	for (GameObject& object : world.GetGameObjects())
	{
		TransformComponent* transform = world.GetTransform(object.id);
		StateComponent* state = world.GetComponent<StateComponent>(object.id);
		VelocityComponent* velocity = world.GetComponent<VelocityComponent>(object.id);
		if (!transform || !state || !velocity)
		{
			continue;
		}

		Vector3 position = TransformSystem::GetLocalPosition(*transform);
		if (position.y <= 0.0f)
		{
			position.y = 0.0f;
			TransformSystem::SetLocalPosition(*transform, position);
			SetVelocityY(*velocity, 0.0f);
			state->isGrounded = true;
			continue;
		}

		state->isGrounded = false;
	}
}

bool MovementSystem::ShouldApplyGravity(const TransformComponent& transform, const StateComponent& state)
{
	if (state.currentActionState == PlayerActionState::Jump
		|| state.currentActionState == PlayerActionState::Fall)
	{
		return true;
	}

	if (!state.isGrounded)
	{
		return true;
	}

	return TransformSystem::GetLocalPosition(transform).y > 0.0f;
}
