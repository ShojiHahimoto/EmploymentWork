#include "World/World.h"

GameObjectId World::CreateTransform()
{
	const GameObjectId objectId = nextObjectId++;
	transforms.emplace(objectId, TransformComponent{});
	return objectId;
}

void World::Clear()
{
	transforms.clear();
	nextObjectId = 1;
	activeCameraId = INVALID_GAME_OBJECT_ID;
	activeCamera = CameraComponent{};
}

TransformSystem::TransformTable& World::GetTransforms()
{
	return transforms;
}

const TransformSystem::TransformTable& World::GetTransforms() const
{
	return transforms;
}

TransformComponent* World::GetTransform(GameObjectId objectId)
{
	auto it = transforms.find(objectId);
	if (it == transforms.end())
	{
		return nullptr;
	}

	return &it->second;
}

const TransformComponent* World::GetTransform(GameObjectId objectId) const
{
	auto it = transforms.find(objectId);
	if (it == transforms.end())
	{
		return nullptr;
	}

	return &it->second;
}

void World::SetActiveCamera(GameObjectId cameraId, const CameraComponent& camera)
{
	activeCameraId = cameraId;
	activeCamera = camera;
}

GameObjectId World::GetActiveCameraId() const
{
	return activeCameraId;
}

CameraComponent& World::GetActiveCamera()
{
	return activeCamera;
}

const CameraComponent& World::GetActiveCamera() const
{
	return activeCamera;
}

bool World::HasActiveCamera() const
{
	return activeCameraId != INVALID_GAME_OBJECT_ID && transforms.find(activeCameraId) != transforms.end();
}
