#include "System/SpawnDestroySystem.h"

#include "Component/ModelComponent.h"
#include "System/TransformSystem.h"
#include "World/World.h"

using namespace DirectX::SimpleMath;

void SpawnDestroySystem::Update(World& world)
{
	ApplyDestroyRequests(world);
	ApplySpawnRequests(world);
}

void SpawnDestroySystem::ApplySpawnRequests(World& world)
{
	const std::vector<SpawnRequest> spawnRequests = world.GetSpawnRequests();

	for (const SpawnRequest& request : spawnRequests)
	{
		switch (request.type)
		{
		case SpawnType::Debugman:
		{
			const GameObjectId objectId = world.CreateTransform(request.name);
			TransformComponent* transform = world.GetTransform(objectId);
			if (transform)
			{
				TransformSystem::SetLocalPosition(*transform, request.position);
				TransformSystem::SetLocalEulerRotationDegrees(*transform, request.rotationDegrees);
				TransformSystem::SetLocalScale(*transform, Vector3(0.02f, 0.02f, 0.02f));
			}

			ModelComponent model;
			model.resourceKey = "Debugman";
			world.AddComponent<ModelComponent>(objectId, model);
			break;
		}
		case SpawnType::DebugCube:
		default:
		{
			const GameObjectId objectId = world.CreateTransform(request.name);
			TransformComponent* transform = world.GetTransform(objectId);
			if (transform)
			{
				TransformSystem::SetLocalPosition(*transform, request.position);
				TransformSystem::SetLocalEulerRotationDegrees(*transform, request.rotationDegrees);
				TransformSystem::SetLocalScale(*transform, Vector3::One);
			}
			break;
		}
		}
	}

	world.ClearSpawnRequests();
}

void SpawnDestroySystem::ApplyDestroyRequests(World& world)
{
	const std::vector<DestroyRequest> destroyRequests = world.GetDestroyRequests();

	for (const DestroyRequest& request : destroyRequests)
	{
		world.DestroyGameObjectImmediate(request.targetId);
	}

	world.ClearDestroyRequests();
}
