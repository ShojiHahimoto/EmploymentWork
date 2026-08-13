#pragma once

#include "Component/CameraComponent.h"
#include "Component/TransformComponent.h"
#include "Core/GameObject.h"
#include "Data/AttackData.h"

#include <array>
#include <cassert>
#include <memory>
#include <string>
#include <utility>
#include <vector>

enum class SpawnType
{
	DebugCube,
	Debugman,
	DebugPlayer,
	DebugPlayer2,
};

enum class BattleResult
{
	None,
	Player1Win,
	Player2Win,
	Draw,
};

struct SpawnRequest
{
	SpawnType type = SpawnType::DebugCube;
	std::string name = "GameObject";
	DirectX::SimpleMath::Vector3 position = DirectX::SimpleMath::Vector3::Zero;
	DirectX::SimpleMath::Vector3 rotationDegrees = DirectX::SimpleMath::Vector3::Zero;
};

struct DestroyRequest
{
	GameObjectId targetId = INVALID_GAME_OBJECT_ID;
};

struct HitCollisionResult
{
	GameObjectId attackerId = INVALID_GAME_OBJECT_ID;
	GameObjectId defenderId = INVALID_GAME_OBJECT_ID;
	std::string attackSlotId;
	std::string attackDataId;
	std::string attackDisplayName;
	int damage = 10;
	int hitstunFrames = 30;
	int guardstunFrames = 30;
	HitReactionType hitReactionType = HitReactionType::Normal;
	AttackUsableState attackUsableState = AttackUsableState::Ground;
	int hitboxIndex = -1;
};

struct HitReactionRequest
{
	GameObjectId attackerId = INVALID_GAME_OBJECT_ID;
	GameObjectId defenderId = INVALID_GAME_OBJECT_ID;
	HitReactionType hitReactionType = HitReactionType::Normal;
	AttackUsableState attackUsableState = AttackUsableState::Ground;
	bool guarded = false;
	bool defenderWasGrounded = true;
};

class World
{
public:
	static constexpr int BattlePlayerCount = 2;

	GameObjectId CreateGameObject();
	GameObjectId CreateGameObject(const std::string& name);

	// 既存コードとの接続用。GameObject を生成し、TransformComponent を追加する。
	GameObjectId CreateTransform();
	GameObjectId CreateTransform(const std::string& name);
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

	void RequestSpawn(SpawnType type, const std::string& name, const DirectX::SimpleMath::Vector3& position, const DirectX::SimpleMath::Vector3& rotationDegrees);
	void RequestDestroy(GameObjectId objectId);

	const std::vector<SpawnRequest>& GetSpawnRequests() const;
	const std::vector<DestroyRequest>& GetDestroyRequests() const;
	void ClearSpawnRequests();
	void ClearDestroyRequests();

	void AddHitCollisionResult(const HitCollisionResult& result);
	const std::vector<HitCollisionResult>& GetHitCollisionResults() const;
	void ClearHitCollisionResults();

	void AddHitReactionRequest(const HitReactionRequest& request);
	const std::vector<HitReactionRequest>& GetHitReactionRequests() const;
	void ClearHitReactionRequests();

	void SetBattlePlayerId(int playerIndex, GameObjectId objectId);
	GameObjectId GetBattlePlayerId(int playerIndex) const;
	GameObjectId GetOpponentBattlePlayerId(GameObjectId objectId) const;

	void SetBattleResult(BattleResult result);
	BattleResult GetBattleResult() const;
	bool HasBattleResult() const;
	void ClearBattleResult();

	void DestroyGameObjectImmediate(GameObjectId objectId);

private:
	std::vector<GameObject> gameObjects;
	std::vector<SpawnRequest> spawnRequests;
	std::vector<DestroyRequest> destroyRequests;
	std::vector<HitCollisionResult> hitCollisionResults;
	std::vector<HitReactionRequest> hitReactionRequests;
	std::array<GameObjectId, BattlePlayerCount> battlePlayerIds = { INVALID_GAME_OBJECT_ID, INVALID_GAME_OBJECT_ID };
	BattleResult battleResult = BattleResult::None;
	GameObjectId nextObjectId = 1;

	GameObjectId activeCameraId = INVALID_GAME_OBJECT_ID;

	void CollectDestroyIdsRecursive(GameObjectId objectId, std::vector<GameObjectId>& destroyIds) const;
	bool ContainsObjectId(const std::vector<GameObjectId>& objectIds, GameObjectId objectId) const;
};

/// <summary>
/// 指定 GameObject に Component を追加し、既に同じ型があれば既存 Component を返す。
/// </summary>
/// <param name="objectId">Component を追加する GameObject の ID。</param>
/// <returns>追加または取得した Component。</returns>
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

/// <summary>
/// 指定 GameObject に初期値付きで Component を追加し、既に同じ型があれば値を上書きする。
/// </summary>
/// <param name="objectId">Component を追加する GameObject の ID。</param>
/// <param name="componentValue">追加または上書きする Component の値。</param>
/// <returns>追加または更新した Component。</returns>
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

/// <summary>
/// 指定 GameObject から指定型の Component を取得する。
/// </summary>
/// <param name="objectId">Component を取得する GameObject の ID。</param>
/// <returns>見つかった Component。存在しない場合は nullptr。</returns>
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

/// <summary>
/// 指定 GameObject から指定型の Component を読み取り専用で取得する。
/// </summary>
/// <param name="objectId">Component を取得する GameObject の ID。</param>
/// <returns>見つかった Component。存在しない場合は nullptr。</returns>
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

/// <summary>
/// 指定 GameObject が指定型の Component を持っているか確認する。
/// </summary>
/// <param name="objectId">確認する GameObject の ID。</param>
/// <returns>Component が存在すれば true。</returns>
template <class T>
bool World::HasComponent(GameObjectId objectId) const
{
	return GetComponent<T>(objectId) != nullptr;
}
