#include "System/HitResolveSystem.h"

#include "Component/HealthComponent.h"
#include "Component/HitBoxComponent.h"
#include "Component/InputHistoryComponent.h"
#include "Component/StateComponent.h"
#include "Core/GameObject.h"
#include "System/Debugger.h"
#include "System/HitStopSystem.h"
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
	/// ガード硬直以外の状態で、後ろ入力による通常ガードを受け付けてよいか確認する。
	/// </summary>
	/// <param name="actionState">防御側の現在 ActionState。</param>
	/// <returns>Idle / Crouch / 歩き / 将来用ガード姿勢なら true。</returns>
	bool CanGuardByActionState(PlayerActionState actionState)
	{
		return actionState == PlayerActionState::Idle
			|| actionState == PlayerActionState::Crouch
			|| actionState == PlayerActionState::StandGuard
			|| actionState == PlayerActionState::CrouchGuard
			|| actionState == PlayerActionState::FrontWalk
			|| actionState == PlayerActionState::BackWalk;
	}

	/// <summary>
	/// 現在の向きに対して、テンキー方向から立ち/しゃがみガード種別を取得する。
	/// </summary>
	/// <param name="facingDirection">防御側の現在向き。</param>
	/// <param name="direction">InputHistoryFrame に保存されたテンキー方向。</param>
	/// <returns>ガード入力があれば GuardType。なければ None。</returns>
	GuardType GetGuardTypeFromDirection(FacingDirection facingDirection, int direction)
	{
		if (facingDirection == FacingDirection::Right)
		{
			if (direction == 4)
			{
				return GuardType::Stand;
			}
			if (direction == 1)
			{
				return GuardType::Crouch;
			}
			return GuardType::None;
		}

		if (direction == 6)
		{
			return GuardType::Stand;
		}
		if (direction == 3)
		{
			return GuardType::Crouch;
		}

		return GuardType::None;
	}

	/// <summary>
	/// 攻撃属性とガード姿勢の組み合わせから、ガード可能か判定する。
	/// </summary>
	/// <param name="attackHeight">攻撃側の上段/中段/下段属性。</param>
	/// <param name="guardType">防御側の立ち/しゃがみガード種別。</param>
	/// <returns>組み合わせ上ガード可能なら true。</returns>
	bool CanGuardAttackHeight(AttackHeight attackHeight, GuardType guardType)
	{
		if (guardType == GuardType::None)
		{
			return false;
		}

		switch (attackHeight)
		{
		case AttackHeight::Mid:
			return guardType == GuardType::Stand;
		case AttackHeight::Low:
			return guardType == GuardType::Crouch;
		case AttackHeight::High:
		default:
			return true;
		}
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
	/// 今回の攻撃接触をどのガード種別として解決できるか確認する。
	/// </summary>
	/// <param name="world">防御側 Component を取得する World。</param>
	/// <param name="result">HitCollisionSystem が収集した攻撃接触結果。</param>
	/// <returns>ガード成立時の GuardType。不成立なら None。</returns>
	GuardType ResolveGuardType(World& world, const HitCollisionResult& result)
	{
		const StateComponent* state = world.GetComponent<StateComponent>(result.defenderId);
		if (!state)
		{
			return GuardType::None;
		}

		// ガード硬直中は後ろ入力の有無に関係なく連続ガードとして扱う。
		if (state->currentActionState == PlayerActionState::StandGuardstun)
		{
			return CanGuardAttackHeight(result.attackHeight, GuardType::Stand)
				? GuardType::Stand
				: GuardType::None;
		}
		if (state->currentActionState == PlayerActionState::CrouchGuardstun)
		{
			return CanGuardAttackHeight(result.attackHeight, GuardType::Crouch)
				? GuardType::Crouch
				: GuardType::None;
		}

		if (!state->isGrounded || !CanGuardByActionState(state->currentActionState))
		{
			return GuardType::None;
		}

		const InputHistoryComponent* inputHistory = world.GetComponent<InputHistoryComponent>(result.defenderId);
		const InputHistoryFrame* inputFrame = GetLatestInputHistoryFrame(inputHistory);
		if (!inputFrame)
		{
			return GuardType::None;
		}

		const GuardType guardType = GetGuardTypeFromDirection(state->facingDirection, inputFrame->direction);
		return CanGuardAttackHeight(result.attackHeight, guardType) ? guardType : GuardType::None;
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

	/// <summary>
	/// 同一フレーム内に攻撃側と防御側が入れ替わったヒット結果があるか確認する。
	/// </summary>
	/// <param name="results">HitCollisionSystem が収集した同一フレームのヒット結果一覧。</param>
	/// <param name="target">相打ち判定したいヒット結果。</param>
	/// <returns>target と逆向きのヒット結果があれば true。</returns>
	bool HasMutualHitResult(
		const std::vector<HitCollisionResult>& results,
		const HitCollisionResult& target)
	{
		for (const HitCollisionResult& result : results)
		{
			if (result.attackerId == target.defenderId
				&& result.defenderId == target.attackerId)
			{
				return true;
			}
		}

		return false;
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
		const GuardType guardType = ResolveGuardType(world, result);
		const bool guarded = guardType != GuardType::None;
		const bool mutualHit = !guarded && HasMutualHitResult(results, result);
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
		HitStopSystem::RequestFromResolvedHit(world, result, guarded, mutualHit);
		if (guarded)
		{
			ApplyGuardstun(world, result.defenderId, result.guardstunFrames, guardType);
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
		if (state->isGrounded)
		{
			state->cameraYFollowMode = CameraYFollowMode::None;
		}

		state->currentActionState = PlayerActionState::Hitstun;
		state->actionFrame = 0;
		state->hitstunDurationFrames = hitstunFrames;
		state->hitstunRequested = false;
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
/// <param name="guardstunFrames">ガード硬直を維持するフレーム数。</param>
/// <param name="guardType">立ち/しゃがみのどちらのガード硬直に入れるか。</param>
void HitResolveSystem::ApplyGuardstun(
	World& world,
	GameObjectId defenderId,
	int guardstunFrames,
	GuardType guardType)
{
	StateComponent* state = world.GetComponent<StateComponent>(defenderId);
	if (state)
	{
		state->cameraYFollowMode = CameraYFollowMode::None;
		state->currentActionState = guardType == GuardType::Crouch
			? PlayerActionState::CrouchGuardstun
			: PlayerActionState::StandGuardstun;
		state->actionFrame = 0;
		state->guardstunDurationFrames = guardstunFrames;
		state->hitstunRequested = false;
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
	request.attackUsableState = result.attackUsableState;
	request.guarded = guarded;
	request.defenderWasGrounded = defenderWasGrounded;
	world.AddHitReactionRequest(request);
}
