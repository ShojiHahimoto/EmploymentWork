#include "System/HitCollisionSystem.h"

#include "Component/CharacterAttackDataComponent.h"
#include "Component/HitBoxComponent.h"
#include "Component/StateComponent.h"
#include "Component/TransformComponent.h"
#include "Data/AttackData.h"
#include "System/TransformSystem.h"
#include "World/World.h"

#include <algorithm>

using namespace DirectX::SimpleMath;

namespace
{
	struct Aabb2D
	{
		float minX = 0.0f;
		float maxX = 0.0f;
		float minY = 0.0f;
		float maxY = 0.0f;
	};

	/// <summary>
	/// Transform と FacingDirection から、指定矩形のワールド上 AABB を作る。
	/// </summary>
	/// <param name="transform">基準座標を持つ TransformComponent。</param>
	/// <param name="state">左右反転に使う StateComponent。</param>
	/// <param name="offset">Transform 位置からの中心オフセット。</param>
	/// <param name="size">AABB の横幅と縦幅。</param>
	/// <returns>X/Y 平面上で扱う AABB。</returns>
	Aabb2D BuildAabb(
		const TransformComponent& transform,
		const StateComponent& state,
		const Vector2& offset,
		const Vector2& size)
	{
		const Vector3 position = TransformSystem::GetLocalPosition(transform);
		const float facingSign = state.facingDirection == FacingDirection::Right ? 1.0f : -1.0f;
		const float centerX = position.x + offset.x * facingSign;
		const float centerY = position.y + offset.y;
		const float halfWidth = size.x * 0.5f;
		const float halfHeight = size.y * 0.5f;

		Aabb2D aabb;
		aabb.minX = centerX - halfWidth;
		aabb.maxX = centerX + halfWidth;
		aabb.minY = centerY - halfHeight;
		aabb.maxY = centerY + halfHeight;
		return aabb;
	}

	/// <summary>
	/// 2 つの AABB が X/Y 両方で重なっているか確認する。
	/// </summary>
	/// <param name="a">比較する AABB。</param>
	/// <param name="b">比較する AABB。</param>
	/// <returns>重なっていれば true。</returns>
	bool IsOverlapping(const Aabb2D& a, const Aabb2D& b)
	{
		return a.minX < b.maxX
			&& a.maxX > b.minX
			&& a.minY < b.maxY
			&& a.maxY > b.minY;
	}

	/// <summary>
	/// 現在の ActionState が攻撃判定を出せる状態か確認する。
	/// </summary>
	/// <param name="state">確認する PlayerActionState。</param>
	/// <returns>地上攻撃または空中攻撃なら true。</returns>
	bool IsAttackState(PlayerActionState state)
	{
		return state == PlayerActionState::GroundAttack
			|| state == PlayerActionState::AirAttack;
	}

	/// <summary>
	/// 防御側が現在攻撃を受け付ける状態か確認する。
	/// </summary>
	/// <param name="state">防御側の StateComponent。</param>
	/// <returns>無敵状態でなければ true。</returns>
	bool CanDefenderReceiveAttack(const StateComponent& state)
	{
		return !state.isInvincible;
	}

	/// <summary>
	/// CharacterAttackDataComponent から指定スロットの技データを探す。
	/// </summary>
	/// <param name="attackData">検索対象の CharacterAttackDataComponent。</param>
	/// <param name="slotId">検索する攻撃スロット ID。</param>
	/// <returns>見つかった割り当て済み技データ。存在しない場合は nullptr。</returns>
	const CharacterAssignedAttackData* FindAssignedAttack(
		const CharacterAttackDataComponent& attackData,
		const std::string& slotId)
	{
		for (const CharacterAssignedAttackData& assignedAttack : attackData.attacks)
		{
			if (assignedAttack.slotId == slotId)
			{
				return &assignedAttack;
			}
		}

		return nullptr;
	}

	/// <summary>
	/// 攻撃判定の矩形が有効な大きさを持っているか確認する。
	/// </summary>
	/// <param name="hitbox">確認する攻撃判定形状。</param>
	/// <returns>横幅と高さが正なら true。</returns>
	bool IsAttackHitboxShapeValid(const AttackHitboxData& hitbox)
	{
		return hitbox.size.x > 0.0f && hitbox.size.y > 0.0f;
	}

	/// <summary>
	/// 攻撃側と防御側の Component が揃っていれば、AttackBox と HurtBox の接触を調べる。
	/// </summary>
	/// <param name="world">ヒット結果を書き込む World。</param>
	/// <param name="attacker">攻撃側 GameObject。</param>
	/// <param name="defender">防御側 GameObject。</param>
	/// <param name="assignedAttack">攻撃側が現在実行している技データ。</param>
	/// <returns>この防御側にヒット結果を追加した場合は true。</returns>
	bool TryCollectHitAgainstDefender(
		World& world,
		const GameObject& attacker,
		const GameObject& defender,
		const CharacterAssignedAttackData& assignedAttack)
	{
		const TransformComponent* attackerTransform = world.GetTransform(attacker.id);
		const StateComponent* attackerState = world.GetComponent<StateComponent>(attacker.id);
		const TransformComponent* defenderTransform = world.GetTransform(defender.id);
		const StateComponent* defenderState = world.GetComponent<StateComponent>(defender.id);
		const HitBoxComponent* defenderHitBox = world.GetComponent<HitBoxComponent>(defender.id);
		if (!attackerTransform || !attackerState || !defenderTransform || !defenderState || !defenderHitBox)
		{
			return false;
		}

		if (!CanDefenderReceiveAttack(*defenderState))
		{
			return false;
		}

		if (!defenderHitBox->hurtBox.enabled
			|| defenderHitBox->hurtBox.size.x <= 0.0f
			|| defenderHitBox->hurtBox.size.y <= 0.0f)
		{
			return false;
		}

		const Aabb2D hurtBoxAabb = BuildAabb(
			*defenderTransform,
			*defenderState,
			defenderHitBox->hurtBox.offset,
			defenderHitBox->hurtBox.size);

		if (!IsAttackFrameActive(assignedAttack.attack.frame, attackerState->actionFrame))
		{
			return false;
		}

		for (size_t hitboxIndex = 0; hitboxIndex < assignedAttack.attack.hitboxes.size(); ++hitboxIndex)
		{
			const AttackHitboxData& attackHitbox = assignedAttack.attack.hitboxes[hitboxIndex];
			if (!IsAttackHitboxShapeValid(attackHitbox))
			{
				continue;
			}

			const Aabb2D attackBoxAabb = BuildAabb(
				*attackerTransform,
				*attackerState,
				attackHitbox.offset,
				attackHitbox.size);

			if (!IsOverlapping(attackBoxAabb, hurtBoxAabb))
			{
				continue;
			}

			HitCollisionResult result;
			result.attackerId = attacker.id;
			result.defenderId = defender.id;
			result.attackSlotId = assignedAttack.slotId;
			result.attackDataId = assignedAttack.attack.attackDataId;
			result.attackDisplayName = assignedAttack.attack.displayName;
			result.damage = assignedAttack.attack.damage;
			result.hitstunFrames = assignedAttack.attack.hitstunFrames;
			result.guardstunFrames = assignedAttack.attack.guardstunFrames;
			result.hitReactionType = assignedAttack.attack.hitReactionType;
			result.hitboxIndex = static_cast<int>(hitboxIndex);
			world.AddHitCollisionResult(result);
			return true;
		}

		return false;
	}
}

void HitCollisionSystem::Update(World& world)
{
	world.ClearHitCollisionResults();

	for (const GameObject& attacker : world.GetGameObjects())
	{
		if (attacker.tag != GameObjectTag::Player)
		{
			continue;
		}

		const StateComponent* attackerState = world.GetComponent<StateComponent>(attacker.id);
		const HitBoxComponent* attackerHitBox = world.GetComponent<HitBoxComponent>(attacker.id);
		const CharacterAttackDataComponent* attackData = world.GetComponent<CharacterAttackDataComponent>(attacker.id);
		if (!attackerState || !attackerHitBox || !attackData)
		{
			continue;
		}

		if (!IsAttackState(attackerState->currentActionState)
			|| attackerHitBox->currentAttack.slotId.empty()
			|| attackerHitBox->currentAttack.hasHit)
		{
			continue;
		}

		const CharacterAssignedAttackData* assignedAttack = FindAssignedAttack(
			*attackData,
			attackerHitBox->currentAttack.slotId);
		if (!assignedAttack)
		{
			continue;
		}

		for (const GameObject& defender : world.GetGameObjects())
		{
			if (defender.id == attacker.id || defender.tag != GameObjectTag::Player)
			{
				continue;
			}

			if (TryCollectHitAgainstDefender(world, attacker, defender, *assignedAttack))
			{
				break;
			}
		}
	}
}
