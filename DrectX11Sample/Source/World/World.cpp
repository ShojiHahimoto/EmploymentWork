#include "World/World.h"

GameObjectId World::CreateGameObject()
{
	const GameObjectId objectId = nextObjectId++;

	GameObject object;
	object.id = objectId;
	gameObjects.push_back(std::move(object));

	return objectId;
}

GameObjectId World::CreateTransform()
{
	const GameObjectId objectId = CreateGameObject();
	AddComponent<TransformComponent>(objectId);
	return objectId;
}

void World::Clear()
{
	gameObjects.clear();
	nextObjectId = 1;
	activeCameraId = INVALID_GAME_OBJECT_ID;
}

std::vector<GameObject>& World::GetGameObjects()
{
	return gameObjects;
}

const std::vector<GameObject>& World::GetGameObjects() const
{
	return gameObjects;
}

GameObject* World::GetGameObject(GameObjectId objectId)
{
	for (GameObject& object : gameObjects)
	{
		if (object.id == objectId)
		{
			return &object;
		}
	}

	return nullptr;
}

const GameObject* World::GetGameObject(GameObjectId objectId) const
{
	for (const GameObject& object : gameObjects)
	{
		if (object.id == objectId)
		{
			return &object;
		}
	}

	return nullptr;
}

TransformComponent* World::GetTransform(GameObjectId objectId)
{
	return GetComponent<TransformComponent>(objectId);
}

const TransformComponent* World::GetTransform(GameObjectId objectId) const
{
	return GetComponent<TransformComponent>(objectId);
}

void World::SetActiveCamera(GameObjectId cameraId, const CameraComponent& camera)
{
	activeCameraId = cameraId;
	AddComponent<CameraComponent>(cameraId, camera);
}

GameObjectId World::GetActiveCameraId() const
{
	return activeCameraId;
}

CameraComponent& World::GetActiveCamera()
{
	CameraComponent* camera = GetComponent<CameraComponent>(activeCameraId);
	assert(camera != nullptr);
	return *camera;
}

const CameraComponent& World::GetActiveCamera() const
{
	const CameraComponent* camera = GetComponent<CameraComponent>(activeCameraId);
	assert(camera != nullptr);
	return *camera;
}

bool World::HasActiveCamera() const
{
	return activeCameraId != INVALID_GAME_OBJECT_ID
		&& GetGameObject(activeCameraId) != nullptr
		&& HasComponent<TransformComponent>(activeCameraId)
		&& HasComponent<CameraComponent>(activeCameraId);
}
