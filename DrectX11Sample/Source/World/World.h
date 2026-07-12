#pragma once

#include "Component/CameraComponent.h"
#include "Component/TransformComponent.h"
#include "Core/GameObject.h"

#include <cassert>
#include <memory>
#include <utility>
#include <vector>

enum class SpawnType
{
	DebugCube,
};

struct SpawnRequest
{
	SpawnType type = SpawnType::DebugCube;
	DirectX::SimpleMath::Vector3 position = DirectX::SimpleMath::Vector3::Zero;
	DirectX::SimpleMath::Vector3 rotationDegrees = DirectX::SimpleMath::Vector3::Zero;
};

struct DestroyRequest
{
	GameObjectId targetId = INVALID_GAME_OBJECT_ID;
};

class World
{
public:
	GameObjectId CreateGameObject();

	// 既存コードとの接続用。GameObject を生成し、TransformComponent を追加する。
	GameObjectId CreateTransform();
	void Clear();

	std::vector<GameObject>& GetGameObjects();
	const std::vector<GameObject>& GetGameObjects() const;

	GameObject* GetGameObject(GameObjectId objectId);
	const GameObject* GetGameObject(GameObjectId objectId) const;

	template <class T>
	T& AddComponent(GameObjectId objectId);

	template <class T>
	T& AddComponent(GameObjectId objectId, const T& componentValue);

	template <class T>
	T* GetComponent(GameObjectId objectId);

	template <class T>
	const T* GetComponent(GameObjectId objectId) const;

	template <class T>
	bool HasComponent(GameObjectId objectId) const;

	TransformComponent* GetTransform(GameObjectId objectId);
	const TransformComponent* GetTransform(GameObjectId objectId) const;

	void SetActiveCamera(GameObjectId cameraId, const CameraComponent& camera);
	GameObjectId GetActiveCameraId() const;
	CameraComponent& GetActiveCamera();
	const CameraComponent& GetActiveCamera() const;
	bool HasActiveCamera() const;

	void RequestSpawn(SpawnType type, const DirectX::SimpleMath::Vector3& position, const DirectX::SimpleMath::Vector3& rotationDegrees);
	void RequestDestroy(GameObjectId objectId);

	const std::vector<SpawnRequest>& GetSpawnRequests() const;
	const std::vector<DestroyRequest>& GetDestroyRequests() const;
	void ClearSpawnRequests();
	void ClearDestroyRequests();

	void DestroyGameObjectImmediate(GameObjectId objectId);

private:
	std::vector<GameObject> gameObjects;
	std::vector<SpawnRequest> spawnRequests;
	std::vector<DestroyRequest> destroyRequests;
	GameObjectId nextObjectId = 1;

	GameObjectId activeCameraId = INVALID_GAME_OBJECT_ID;

	void CollectDestroyIdsRecursive(GameObjectId objectId, std::vector<GameObjectId>& destroyIds) const;
	bool ContainsObjectId(const std::vector<GameObjectId>& objectIds, GameObjectId objectId) const;
};

template <class T>
T& World::AddComponent(GameObjectId objectId)
{
	GameObject* object = GetGameObject(objectId);
	assert(object != nullptr);

	for (std::unique_ptr<Component>& component : object->components)
	{
		if (T* existing = dynamic_cast<T*>(component.get()))
		{
			return *existing;
		}
	}

	object->components.push_back(std::make_unique<T>());
	return *dynamic_cast<T*>(object->components.back().get());
}

template <class T>
T& World::AddComponent(GameObjectId objectId, const T& componentValue)
{
	GameObject* object = GetGameObject(objectId);
	assert(object != nullptr);

	for (std::unique_ptr<Component>& component : object->components)
	{
		if (T* existing = dynamic_cast<T*>(component.get()))
		{
			*existing = componentValue;
			return *existing;
		}
	}

	object->components.push_back(std::make_unique<T>(componentValue));
	return *dynamic_cast<T*>(object->components.back().get());
}

template <class T>
T* World::GetComponent(GameObjectId objectId)
{
	GameObject* object = GetGameObject(objectId);
	if (!object)
	{
		return nullptr;
	}

	for (std::unique_ptr<Component>& component : object->components)
	{
		if (T* typedComponent = dynamic_cast<T*>(component.get()))
		{
			return typedComponent;
		}
	}

	return nullptr;
}

template <class T>
const T* World::GetComponent(GameObjectId objectId) const
{
	const GameObject* object = GetGameObject(objectId);
	if (!object)
	{
		return nullptr;
	}

	for (const std::unique_ptr<Component>& component : object->components)
	{
		if (const T* typedComponent = dynamic_cast<const T*>(component.get()))
		{
			return typedComponent;
		}
	}

	return nullptr;
}

template <class T>
bool World::HasComponent(GameObjectId objectId) const
{
	return GetComponent<T>(objectId) != nullptr;
}
