#include "System/SpawnDestroySystem.h"

#include "Component/CharacterAttackDataComponent.h"
#include "Component/CharacterParameterComponent.h"
#include "Component/CommandBufferComponent.h"
#include "Component/HitBoxComponent.h"
#include "Component/HealthComponent.h"
#include "Component/InputHistoryComponent.h"
#include "Component/ModelComponent.h"
#include "Component/SkeletonPoseComponent.h"
#include "Component/StateComponent.h"
#include "Component/VelocityComponent.h"
#include "Data/BattleSetupData.h"
#include "Data/CharacterDataLoader.h"
#include "System/TransformSystem.h"
#include "World/World.h"

using namespace DirectX::SimpleMath;

namespace
{
	/// <summary>
	/// デバッグ用 Player が共通して持つ Component を追加する。
	/// </summary>
	/// <param name="world">Component を追加する World。</param>
	/// <param name="objectId">初期化対象の Player GameObject ID。</param>
	/// <param name="request">Spawn 時の位置と回転を持つリクエスト。</param>
	/// <param name="modelKey">描画に使う ModelResource のキー。</param>
	/// <param name="initialFacingDirection">初期の対面方向。</param>
	void InitializeDebugPlayer(
		World& world,
		GameObjectId objectId,
		const SpawnRequest& request,
		const std::string& modelKey,
		FacingDirection initialFacingDirection)
	{
		if (GameObject* object = world.GetGameObject(objectId))
		{
			object->tag = GameObjectTag::Player;
		}

		const std::string characterFolderPath = request.characterFolderPath.empty()
			? BattleSetup::BuildCharacterFolderPath(0)
			: request.characterFolderPath;

		CharacterData characterData;
		CharacterDataLoader::LoadCharacterData(characterFolderPath, characterData);

		TransformComponent* transform = world.GetTransform(objectId);
		if (transform)
		{
			TransformSystem::SetLocalPosition(*transform, request.position);
			TransformSystem::SetLocalEulerRotationDegrees(
				*transform,
				initialFacingDirection == FacingDirection::Right
					? Vector3(0.0f, -90.0f, 0.0f)
					: Vector3(0.0f, 90.0f, 0.0f));
			TransformSystem::SetLocalScale(*transform, characterData.parameter.modelScale);
		}

		ModelComponent model;
		model.resourceKey = modelKey;
		world.AddComponent<ModelComponent>(objectId, model);

		SkeletonPoseComponent skeletonPose;
		skeletonPose.enableDebugPose = true;
		world.AddComponent<SkeletonPoseComponent>(objectId, skeletonPose);

		world.AddComponent<VelocityComponent>(objectId);

		StateComponent state;
		state.facingDirection = initialFacingDirection;
		world.AddComponent<StateComponent>(objectId, state);

		CharacterParameterComponent characterParameter;
		characterParameter.parameter = characterData.parameter;
		world.AddComponent<CharacterParameterComponent>(objectId, characterParameter);

		HealthComponent health;
		health.maxHp = characterData.parameter.maxHp > 0 ? characterData.parameter.maxHp : 1;
		health.currentHp = health.maxHp;
		world.AddComponent<HealthComponent>(objectId, health);

		CharacterAttackDataComponent characterAttackData;
		characterAttackData.attacks = characterData.attacks;
		world.AddComponent<CharacterAttackDataComponent>(objectId, characterAttackData);

		HitBoxComponent hitBox;
		hitBox.pushBox.offset = characterData.parameter.pushBox.offset;
		hitBox.pushBox.size = characterData.parameter.pushBox.size;
		hitBox.hurtBox.offset = characterData.parameter.hurtBox.offset;
		hitBox.hurtBox.size = characterData.parameter.hurtBox.size;
		world.AddComponent<HitBoxComponent>(objectId, hitBox);

		world.AddComponent<InputHistoryComponent>(objectId);
		world.AddComponent<CommandBufferComponent>(objectId);
	}
}

void SpawnDestroySystem::Update(World& world)
{
	ApplyDestroyRequests(world);
	ApplySpawnRequests(world);
}

/// <summary>
/// World に蓄積された SpawnRequest を処理し、種類ごとの初期 Component を持つ GameObject を生成する。
/// </summary>
/// <param name="world">生成リクエストと GameObject を保持する World。</param>
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
				TransformSystem::SetLocalScale(*transform, Vector3(0.05f, 0.05f, 0.05f));
			}

			ModelComponent model;
			model.resourceKey = "Debugman";
			world.AddComponent<ModelComponent>(objectId, model);

			SkeletonPoseComponent skeletonPose;
			skeletonPose.enableDebugPose = true;
			world.AddComponent<SkeletonPoseComponent>(objectId, skeletonPose);
			break;
		}
		case SpawnType::DebugPlayer:
		{
			const GameObjectId objectId = world.CreateTransform(request.name);
			InitializeDebugPlayer(world, objectId, request, "DebugPlayer", FacingDirection::Right);
			world.SetBattlePlayerId(0, objectId);
			break;
		}
		case SpawnType::DebugPlayer2:
		{
			const GameObjectId objectId = world.CreateTransform(request.name);
			InitializeDebugPlayer(world, objectId, request, "DebugPlayer2", FacingDirection::Left);
			world.SetBattlePlayerId(1, objectId);
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

/// <summary>
/// World に蓄積された DestroyRequest を処理し、対象 GameObject を削除する。
/// </summary>
/// <param name="world">削除リクエストと GameObject を保持する World。</param>
void SpawnDestroySystem::ApplyDestroyRequests(World& world)
{
	const std::vector<DestroyRequest> destroyRequests = world.GetDestroyRequests();

	for (const DestroyRequest& request : destroyRequests)
	{
		world.DestroyGameObjectImmediate(request.targetId);
	}

	world.ClearDestroyRequests();
}
