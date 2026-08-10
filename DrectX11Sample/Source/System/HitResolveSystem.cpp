#include "System/HitResolveSystem.h"

#include "Component/HealthComponent.h"
#include "Component/HitBoxComponent.h"
#include "Component/InputHistoryComponent.h"
#include "Component/StateComponent.h"
#include "Core/GameObject.h"
#include "System/Debugger.h"
#include "World/World.h"

#include <vector>

namespace
{
	constexpr int GuardDamageDivisor = 10;

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

	/// <summary>
	/// Guardstun 以外の状態で、後ろ入力による通常ガードを受け付けてよいか確認する。
	/// </summary>
	/// <param name="actionState">防御側の現在 ActionState。</param>
	/// <returns>Idle / FrontWalk / BackWalk なら true。</returns>
	bool CanGuardByActionState(PlayerActionState actionState)
	{
		return actionState == PlayerActionState::Idle
			|| actionState == PlayerActionState::FrontWalk
			|| actionState == PlayerActionState::BackWalk;
	}

	/// <summary>
	/// 現在の向きに対して、テンキー方向が後ろ入力になっているか確認する。
	/// </summary>
	/// <param name="facingDirection">防御側の現在向き。</param>
	/// <param name="direction">InputHistoryFrame に保存されたテンキー方向。</param>
	/// <returns>右向きなら 4 / 1、左向きなら 6 / 3 の場合 true。</returns>
	bool IsHoldingBackDirection(FacingDirection facingDirection, int direction)
	{
		if (facingDirection == FacingDirection::Right)
		{
			return direction == 4 || direction == 1;
		}

		return direction == 6 || direction == 3;
	}

	/// <summary>
	/// InputHistoryComponent から最新フレームの入力履歴を取得する。
	/// </summary>
	/// <param name="inputHistory">検索対象の InputHistoryComponent。</param>
	/// <returns>最新入力履歴。存在しない場合は nullptr。</returns>
	const InputHistoryFrame* GetLatestInputHistoryFrame(const InputHistoryComponent* inputHistory)
	{
		if (!inputHistory
			|| inputHistory->latestFrameIndex < 0
			|| inputHistory->storedFrameCount <= 0)
		{
			return nullptr;
		}

		return &inputHistory->frames[inputHistory->latestFrameIndex];
	}

	/// <summary>
	/// 今回の攻撃接触をガードとして解決できるか確認する。
	/// </summary>
	/// <param name="world">防御側 Component を取得する World。</param>
	/// <param name="result">HitCollisionSystem が収集した攻撃接触結果。</param>
	/// <returns>ガード成立なら true。</returns>
	bool ShouldResolveAsGuard(World& world, const HitCollisionResult& result)
	{
		const StateComponent* state = world.GetComponent<StateComponent>(result.defenderId);
		if (!state)
		{
			return false;
		}

		// ガード硬直中は後ろ入力の有無に関係なく連続ガードとして扱う。
		if (state->currentActionState == PlayerActionState::Guardstun)
		{
			return true;
		}

		if (!state->isGrounded || !CanGuardByActionState(state->currentActionState))
		{
			return false;
		}

		const InputHistoryComponent* inputHistory = world.GetComponent<InputHistoryComponent>(result.defenderId);
		const InputHistoryFrame* inputFrame = GetLatestInputHistoryFrame(inputHistory);
		return inputFrame && IsHoldingBackDirection(state->facingDirection, inputFrame->direction);
	}

	/// <summary>
	/// ガード時の削りダメージを計算する。
	/// </summary>
	/// <param name="damage">本来の攻撃ダメージ。</param>
	/// <returns>本来ダメージの 1/10。0 以下の攻撃なら 0。</returns>
	int CalculateGuardDamage(int damage)
	{
		if (damage <= 0)
		{
			return 0;
		}

		return damage / GuardDamageDivisor;
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
		MarkAttackAsHit(world, result.attackerId);
	}

	for (const HitCollisionResult& result : results)
	{
		const bool guarded = ShouldResolveAsGuard(world, result);
		const int resolvedDamage = guarded ? CalculateGuardDamage(result.damage) : result.damage;
		const StateComponent* defenderState = world.GetComponent<StateComponent>(result.defenderId);
		const bool defenderWasGrounded = defenderState ? defenderState->isGrounded : true;

		DebugLog(
			guarded ? "[Guard] " : "[Hit] ",
			GetObjectNameOrUnknown(world, result.attackerId),
			" -> ",
			GetObjectNameOrUnknown(world, result.defenderId),
			" slot=",
			result.attackSlotId,
			" attack=",
			result.attackDisplayName.empty() ? result.attackDataId : result.attackDisplayName,
			" damage=",
			resolvedDamage,
			guarded ? " guardstunFrames=" : " hitstunFrames=",
			guarded ? result.guardstunFrames : result.hitstunFrames,
			" hitbox=",
			result.hitboxIndex);

		ApplyDamage(world, result.defenderId, resolvedDamage);
		if (guarded)
		{
			ApplyGuardstun(world, result.defenderId, result.guardstunFrames);
		}
		else
		{
			ApplyHitstun(world, result.defenderId, result.hitstunFrames);
		}

		QueueHitReaction(world, result, guarded, defenderWasGrounded);
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

/// <summary>
/// 防御側をガード硬直状態へ遷移させ、実行中の攻撃情報を消す。
/// </summary>
/// <param name="world">防御側 Component を取得する World。</param>
/// <param name="defenderId">防御側 GameObject ID。</param>
/// <param name="guardstunFrames">Guardstun を維持するフレーム数。</param>
void HitResolveSystem::ApplyGuardstun(World& world, GameObjectId defenderId, int guardstunFrames)
{
	StateComponent* state = world.GetComponent<StateComponent>(defenderId);
	if (state)
	{
		state->currentActionState = PlayerActionState::Guardstun;
		state->actionFrame = 0;
		state->guardstunDurationFrames = guardstunFrames;
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

/// <summary>
/// 確定済みのヒットまたはガードから、位置補正や吹き飛び用の被弾反応リクエストを作る。
/// </summary>
/// <param name="world">リクエストを蓄積する World。</param>
/// <param name="result">HitCollisionSystem が収集した攻撃接触結果。</param>
/// <param name="guarded">ガードとして解決された場合は true。</param>
/// <param name="defenderWasGrounded">HitResolve 前の防御側が接地していた場合は true。</param>
void HitResolveSystem::QueueHitReaction(
	World& world,
	const HitCollisionResult& result,
	bool guarded,
	bool defenderWasGrounded)
{
	HitReactionRequest request;
	request.attackerId = result.attackerId;
	request.defenderId = result.defenderId;
	request.hitReactionType = guarded ? HitReactionType::Normal : result.hitReactionType;
	request.guarded = guarded;
	request.defenderWasGrounded = defenderWasGrounded;
	world.AddHitReactionRequest(request);
}
