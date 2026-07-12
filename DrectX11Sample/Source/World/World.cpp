#include "World/World.h"

#include <algorithm>

GameObjectId World::CreateGameObject()
{
	return CreateGameObject("GameObject");
}

GameObjectId World::CreateGameObject(const std::string& name)
{
	const GameObjectId objectId = nextObjectId++;

	GameObject object;
	object.id = objectId;
	object.name = name;
	gameObjects.push_back(std::move(object));

	return objectId;
}

GameObjectId World::CreateTransform()
{
	return CreateTransform("GameObject");
}

GameObjectId World::CreateTransform(const std::string& name)
{
	const GameObjectId objectId = CreateGameObject(name);
	AddComponent<TransformComponent>(objectId);
	return objectId;
}

void World::Clear()
{
	gameObjects.clear();
	spawnRequests.clear();
	destroyRequests.clear();
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

void World::RequestSpawn(SpawnType type, const std::string& name, const DirectX::SimpleMath::Vector3& position, const DirectX::SimpleMath::Vector3& rotationDegrees)
{
	SpawnRequest request;
	request.type = type;
	request.name = name.empty() ? "GameObject" : name;
	request.position = position;
	request.rotationDegrees = rotationDegrees;
	spawnRequests.push_back(request);
}

void World::RequestDestroy(GameObjectId objectId)
{
	if (objectId == INVALID_GAME_OBJECT_ID)
	{
		return;
	}

	DestroyRequest request;
	request.targetId = objectId;
	destroyRequests.push_back(request);
}

const std::vector<SpawnRequest>& World::GetSpawnRequests() const
{
	return spawnRequests;
}

const std::vector<DestroyRequest>& World::GetDestroyRequests() const
{
	return destroyRequests;
}

void World::ClearSpawnRequests()
{
	spawnRequests.clear();
}

void World::ClearDestroyRequests()
{
	destroyRequests.clear();
}

void World::DestroyGameObjectImmediate(GameObjectId objectId)
{
	if (objectId == INVALID_GAME_OBJECT_ID)
	{
		return;
	}

	std::vector<GameObjectId> destroyIds;
	CollectDestroyIdsRecursive(objectId, destroyIds);
	if (destroyIds.empty())
	{
		return;
	}

	if (ContainsObjectId(destroyIds, activeCameraId))
	{
		activeCameraId = INVALID_GAME_OBJECT_ID;
	}

	for (GameObject& object : gameObjects)
	{
		if (ContainsObjectId(destroyIds, object.id))
		{
			continue;
		}

		TransformComponent* transform = GetComponent<TransformComponent>(object.id);
		if (!transform)
		{
			continue;
		}

		if (ContainsObjectId(destroyIds, transform->parentId))
		{
			transform->parentId = INVALID_GAME_OBJECT_ID;
			transform->dirty = true;
		}

		transform->childIds.erase(
			std::remove_if(
				transform->childIds.begin(),
				transform->childIds.end(),
				[this, &destroyIds](GameObjectId childId)
				{
					return ContainsObjectId(destroyIds, childId);
				}),
			transform->childIds.end());
	}

	gameObjects.erase(
		std::remove_if(
			gameObjects.begin(),
			gameObjects.end(),
			[this, &destroyIds](const GameObject& object)
			{
				return ContainsObjectId(destroyIds, object.id);
			}),
		gameObjects.end());
}

void World::CollectDestroyIdsRecursive(GameObjectId objectId, std::vector<GameObjectId>& destroyIds) const
{
	if (objectId == INVALID_GAME_OBJECT_ID || ContainsObjectId(destroyIds, objectId))
	{
		return;
	}

	const GameObject* object = GetGameObject(objectId);
	if (!object)
	{
		return;
	}

	destroyIds.push_back(objectId);

	const TransformComponent* transform = GetComponent<TransformComponent>(objectId);
	if (!transform)
	{
		return;
	}

	for (GameObjectId childId : transform->childIds)
	{
		CollectDestroyIdsRecursive(childId, destroyIds);
	}
}

bool World::ContainsObjectId(const std::vector<GameObjectId>& objectIds, GameObjectId objectId) const
{
	return std::find(objectIds.begin(), objectIds.end(), objectId) != objectIds.end();
}
