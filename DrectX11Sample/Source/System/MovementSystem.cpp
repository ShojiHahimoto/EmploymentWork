#include "System/MovementSystem.h"

#include "Component/TransformComponent.h"
#include "Component/VelocityComponent.h"
#include "System/TransformSystem.h"
#include "World/World.h"

using namespace DirectX::SimpleMath;

void MovementSystem::Update(World& world)
{
	ApplyVelocityToTransform(world);
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
