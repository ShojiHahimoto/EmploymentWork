#include "System/HitResolveSystem.h"

#include "Component/HealthComponent.h"
#include "Component/HitBoxComponent.h"
#include "Component/StateComponent.h"
#include "Core/GameObject.h"
#include "System/Debugger.h"
#include "World/World.h"

#include <vector>

namespace
{
	/// <summary>
	/// ログ表示用に GameObject 名を取得する。
	/// </summary>
	/// <param name="world">検索対象の World。</param>
	/// <param name="objectId">名前を取得する GameObject ID。</param>
	/// <returns>GameObject 名。存在しない場合は Unknown。</returns>
	const char* GetObjectNameOrUnknown(const World& world, GameObjectId objectId)
	{
		const GameObject* object = world.GetGameObject(objectId);
		if (!object)
		{
			return "Unknown";
		}

		return object->name.c_str();
	}
}

void HitResolveSystem::Update(World& world)
{
	const std::vector<HitCollisionResult> results = world.GetHitCollisionResults();
	if (results.empty())
	{
		world.ClearHitCollisionResults();
		return;
	}

	for (const HitCollisionResult& result : results)
	{
		DebugLog(
			"[Hit] ",
			GetObjectNameOrUnknown(world, result.attackerId),
			" -> ",
			GetObjectNameOrUnknown(world, result.defenderId),
			" slot=",
			result.attackSlotId,
			" attack=",
			result.attackDisplayName.empty() ? result.attackDataId : result.attackDisplayName,
			" damage=",
			result.damage,
			" hitstunFrames=",
			result.hitstunFrames,
			" hitbox=",
			result.hitboxIndex);
	}

	for (const HitCollisionResult& result : results)
	{
		MarkAttackAsHit(world, result.attackerId);
	}

	for (const HitCollisionResult& result : results)
	{
		ApplyDamage(world, result.defenderId, result.damage);
		ApplyHitstun(world, result.defenderId, result.hitstunFrames);
	}

	world.ClearHitCollisionResults();
}

/// <summary>
/// 攻撃側の currentAttack に、今回の攻撃が既にヒット済みであることを記録する。
/// </summary>
/// <param name="world">攻撃側 HitBoxComponent を取得する World。</param>
/// <param name="attackerId">攻撃側 GameObject ID。</param>
void HitResolveSystem::MarkAttackAsHit(World& world, GameObjectId attackerId)
{
	HitBoxComponent* hitBox = world.GetComponent<HitBoxComponent>(attackerId);
	if (!hitBox)
	{
		return;
	}

	hitBox->currentAttack.hasHit = true;
}

/// <summary>
/// 防御側 HP を攻撃力分だけ減算し、0 未満にならないように丸める。
/// </summary>
/// <param name="world">防御側 HealthComponent を取得する World。</param>
/// <param name="defenderId">防御側 GameObject ID。</param>
/// <param name="damage">減算する攻撃力。</param>
void HitResolveSystem::ApplyDamage(World& world, GameObjectId defenderId, int damage)
{
	if (damage <= 0)
	{
		return;
	}

	HealthComponent* health = world.GetComponent<HealthComponent>(defenderId);
	if (!health)
	{
		return;
	}

	health->currentHp -= damage;
	if (health->currentHp < 0)
	{
		health->currentHp = 0;
	}

	DebugLog(
		"[Health] ",
		GetObjectNameOrUnknown(world, defenderId),
		" HP=",
		health->currentHp,
		"/",
		health->maxHp);
}

/// <summary>
/// 防御側を被弾状態へ遷移させ、実行中の攻撃情報を消す。
/// </summary>
/// <param name="world">防御側 Component を取得する World。</param>
/// <param name="defenderId">防御側 GameObject ID。</param>
/// <param name="hitstunFrames">Hitstun を維持するフレーム数。</param>
void HitResolveSystem::ApplyHitstun(World& world, GameObjectId defenderId, int hitstunFrames)
{
	StateComponent* state = world.GetComponent<StateComponent>(defenderId);
	if (state)
	{
		state->currentActionState = PlayerActionState::Hitstun;
		state->actionFrame = 0;
		state->hitstunDurationFrames = hitstunFrames;
		state->hitstunRequested = false;
		state->cancelEnabled = false;
	}

	HitBoxComponent* hitBox = world.GetComponent<HitBoxComponent>(defenderId);
	if (hitBox)
	{
		hitBox->currentAttack.slotId.clear();
		hitBox->currentAttack.hasHit = false;
	}
}
