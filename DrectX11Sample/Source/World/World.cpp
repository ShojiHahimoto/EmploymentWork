#include "World/World.h"

#include <algorithm>

/// <summary>
/// 既定名で GameObject を生成する。
/// </summary>
/// <returns>生成した GameObject の ID。</returns>
GameObjectId World::CreateGameObject()
{
	return CreateGameObject("GameObject");
}

/// <summary>
/// 指定名を持つ GameObject を World に追加する。
/// </summary>
/// <param name="name">生成する GameObject の表示名。</param>
/// <returns>生成した GameObject の ID。</returns>
GameObjectId World::CreateGameObject(const std::string& name)
{
	const GameObjectId objectId = nextObjectId++;

	GameObject object;
	object.id = objectId;
	object.name = name;
	gameObjects.push_back(std::move(object));

	return objectId;
}

/// <summary>
/// 既定名で GameObject を生成し、TransformComponent を追加する。
/// </summary>
/// <returns>生成した GameObject の ID。</returns>
GameObjectId World::CreateTransform()
{
	return CreateTransform("GameObject");
}

/// <summary>
/// 指定名の GameObject を生成し、TransformComponent を追加する。
/// </summary>
/// <param name="name">生成する GameObject の表示名。</param>
/// <returns>生成した GameObject の ID。</returns>
GameObjectId World::CreateTransform(const std::string& name)
{
	const GameObjectId objectId = CreateGameObject(name);
	AddComponent<TransformComponent>(objectId);
	return objectId;
}

/// <summary>
/// World が保持する GameObject、生成削除リクエスト、アクティブカメラ状態を破棄する。
/// </summary>
void World::Clear()
{
	gameObjects.clear();
	spawnRequests.clear();
	destroyRequests.clear();
	nextObjectId = 1;
	activeCameraId = INVALID_GAME_OBJECT_ID;
}

/// <summary>
/// World が保持している GameObject 配列を取得する。
/// </summary>
/// <returns>変更可能な GameObject 配列。</returns>
std::vector<GameObject>& World::GetGameObjects()
{
	return gameObjects;
}

/// <summary>
/// World が保持している GameObject 配列を読み取り専用で取得する。
/// </summary>
/// <returns>読み取り専用の GameObject 配列。</returns>
const std::vector<GameObject>& World::GetGameObjects() const
{
	return gameObjects;
}

/// <summary>
/// 指定 ID の GameObject を検索する。
/// </summary>
/// <param name="objectId">検索する GameObject の ID。</param>
/// <returns>見つかった GameObject。存在しない場合は nullptr。</returns>
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

/// <summary>
/// 指定 ID の GameObject を読み取り専用で検索する。
/// </summary>
/// <param name="objectId">検索する GameObject の ID。</param>
/// <returns>見つかった GameObject。存在しない場合は nullptr。</returns>
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

/// <summary>
/// 指定 GameObject の TransformComponent を取得する。
/// </summary>
/// <param name="objectId">TransformComponent を取得する GameObject の ID。</param>
/// <returns>TransformComponent。存在しない場合は nullptr。</returns>
TransformComponent* World::GetTransform(GameObjectId objectId)
{
	return GetComponent<TransformComponent>(objectId);
}

/// <summary>
/// 指定 GameObject の TransformComponent を読み取り専用で取得する。
/// </summary>
/// <param name="objectId">TransformComponent を取得する GameObject の ID。</param>
/// <returns>読み取り専用の TransformComponent。存在しない場合は nullptr。</returns>
const TransformComponent* World::GetTransform(GameObjectId objectId) const
{
	return GetComponent<TransformComponent>(objectId);
}

/// <summary>
/// 指定 GameObject を World のアクティブカメラとして登録する。
/// </summary>
/// <param name="cameraId">アクティブカメラにする GameObject の ID。</param>
/// <param name="camera">登録する CameraComponent の初期値。</param>
void World::SetActiveCamera(GameObjectId cameraId, const CameraComponent& camera)
{
	activeCameraId = cameraId;
	AddComponent<CameraComponent>(cameraId, camera);
}

/// <summary>
/// 現在のアクティブカメラ GameObject ID を取得する。
/// </summary>
/// <returns>アクティブカメラの ID。未設定の場合は INVALID_GAME_OBJECT_ID。</returns>
GameObjectId World::GetActiveCameraId() const
{
	return activeCameraId;
}

/// <summary>
/// アクティブカメラの CameraComponent を取得する。
/// </summary>
/// <returns>変更可能な CameraComponent。</returns>
CameraComponent& World::GetActiveCamera()
{
	CameraComponent* camera = GetComponent<CameraComponent>(activeCameraId);
	assert(camera != nullptr);
	return *camera;
}

/// <summary>
/// アクティブカメラの CameraComponent を読み取り専用で取得する。
/// </summary>
/// <returns>読み取り専用の CameraComponent。</returns>
const CameraComponent& World::GetActiveCamera() const
{
	const CameraComponent* camera = GetComponent<CameraComponent>(activeCameraId);
	assert(camera != nullptr);
	return *camera;
}

/// <summary>
/// World が有効なアクティブカメラを持っているか確認する。
/// </summary>
/// <returns>カメラ GameObject、TransformComponent、CameraComponent が揃っていれば true。</returns>
bool World::HasActiveCamera() const
{
	return activeCameraId != INVALID_GAME_OBJECT_ID
		&& GetGameObject(activeCameraId) != nullptr
		&& HasComponent<TransformComponent>(activeCameraId)
		&& HasComponent<CameraComponent>(activeCameraId);
}

/// <summary>
/// フレーム終端で生成する GameObject のリクエストを追加する。
/// </summary>
/// <param name="type">生成する GameObject の種類。</param>
/// <param name="name">生成する GameObject の表示名。空の場合は既定名を使う。</param>
/// <param name="position">生成時のローカル座標。</param>
/// <param name="rotationDegrees">生成時のローカル回転角度。</param>
void World::RequestSpawn(SpawnType type, const std::string& name, const DirectX::SimpleMath::Vector3& position, const DirectX::SimpleMath::Vector3& rotationDegrees)
{
	SpawnRequest request;
	request.type = type;
	request.name = name.empty() ? "GameObject" : name;
	request.position = position;
	request.rotationDegrees = rotationDegrees;
	spawnRequests.push_back(request);
}

/// <summary>
/// フレーム終端で削除する GameObject のリクエストを追加する。
/// </summary>
/// <param name="objectId">削除対象の GameObject ID。</param>
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

/// <summary>
/// 蓄積されている生成リクエストを取得する。
/// </summary>
/// <returns>読み取り専用の生成リクエスト配列。</returns>
const std::vector<SpawnRequest>& World::GetSpawnRequests() const
{
	return spawnRequests;
}

/// <summary>
/// 蓄積されている削除リクエストを取得する。
/// </summary>
/// <returns>読み取り専用の削除リクエスト配列。</returns>
const std::vector<DestroyRequest>& World::GetDestroyRequests() const
{
	return destroyRequests;
}

/// <summary>
/// 生成リクエストをすべて破棄する。
/// </summary>
void World::ClearSpawnRequests()
{
	spawnRequests.clear();
}

/// <summary>
/// 削除リクエストをすべて破棄する。
/// </summary>
void World::ClearDestroyRequests()
{
	destroyRequests.clear();
}

/// <summary>
/// 指定 GameObject とその子孫を、リクエストを介さず即時削除する。
/// </summary>
/// <param name="objectId">削除対象の GameObject ID。</param>
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

/// <summary>
/// 指定 GameObject と子孫の ID を、削除対象リストへ再帰的に追加する。
/// </summary>
/// <param name="objectId">起点となる GameObject の ID。</param>
/// <param name="destroyIds">削除対象 ID を蓄積する配列。</param>
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

/// <summary>
/// 指定 ID が ID 配列に含まれているか確認する。
/// </summary>
/// <param name="objectIds">検索対象の ID 配列。</param>
/// <param name="objectId">含まれているか確認する ID。</param>
/// <returns>配列に ID が含まれていれば true。</returns>
bool World::ContainsObjectId(const std::vector<GameObjectId>& objectIds, GameObjectId objectId) const
{
	return std::find(objectIds.begin(), objectIds.end(), objectId) != objectIds.end();
}
