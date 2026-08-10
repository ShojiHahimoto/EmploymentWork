#include "System/HitReactionSystem.h"

#include "Component/HitBoxComponent.h"
#include "Component/StateComponent.h"
#include "Component/TransformComponent.h"
#include "Component/VelocityComponent.h"
#include "System/EmbedResolveSystem.h"
#include "System/TransformSystem.h"
#include "World/World.h"

#include <cmath>
#include <vector>

using namespace DirectX::SimpleMath;

namespace
{
	constexpr float Epsilon = 0.0001f;
	constexpr float NormalBackDistance = 0.6f;
	constexpr int DefaultDownFrames = 30;

	struct Aabb2D
	{
		float minX = 0.0f;
		float maxX = 0.0f;
		float minY = 0.0f;
		float maxY = 0.0f;
	};

	struct PlayerReactionRuntime
	{
		GameObjectId objectId = INVALID_GAME_OBJECT_ID;
		TransformComponent* transform = nullptr;
		StateComponent* state = nullptr;
		HitBoxComponent* hitBox = nullptr;
		VelocityComponent* velocity = nullptr;
	};

	struct HitReactionSetting
	{
		float groundBackDistance = NormalBackDistance;
		Vector3 airVelocity = Vector3::Zero;
		int downFrames = DefaultDownFrames;
	};

	/// <summary>
	/// 被弾反応タイプから、実際に使う距離や速度の共通設定を取得する。
	/// </summary>
	/// <param name="reactionType">AttackData が指定した被弾反応タイプ。</param>
	/// <returns>HitReactionSystem が使う共通設定。</returns>
	HitReactionSetting GetReactionSetting(HitReactionType reactionType)
	{
		HitReactionSetting setting;
		switch (reactionType)
		{
		case HitReactionType::Down:
			setting.groundBackDistance = 0.0f;
			setting.downFrames = DefaultDownFrames;
			break;
		case HitReactionType::Burst:
			setting.airVelocity = Vector3(0.14f, 0.56f, 0.0f);
			break;
		case HitReactionType::HardBurst:
			setting.airVelocity = Vector3(0.28f, 0.32f, 0.0f);
			break;
		case HitReactionType::Normal:
		case HitReactionType::Unknown:
		default:
			setting.groundBackDistance = NormalBackDistance;
			break;
		}

		return setting;
	}

	/// <summary>
	/// 空中追撃時に使う、通常 Burst より弱い再打ち上げ速度を取得する。
	/// </summary>
	/// <returns>空中追撃用の X/Y 速度。</returns>
	Vector3 GetAirFollowupVelocity()
	{
		return Vector3(0.08f, 0.26f, 0.0f);
	}

	/// <summary>
	/// Transform と FacingDirection から PushBox のワールド上 AABB を作る。
	/// </summary>
	/// <param name="transform">基準座標を持つ TransformComponent。</param>
	/// <param name="state">左右反転に使う StateComponent。</param>
	/// <param name="box">AABB 化する PushBox。</param>
	/// <returns>X/Y 平面上の AABB。</returns>
	Aabb2D BuildAabb(const TransformComponent& transform, const StateComponent& state, const HitBoxRect2D& box)
	{
		const Vector3 position = TransformSystem::GetLocalPosition(transform);
		const float facingSign = state.facingDirection == FacingDirection::Right ? 1.0f : -1.0f;
		const float centerX = position.x + box.offset.x * facingSign;
		const float centerY = position.y + box.offset.y;
		const float halfWidth = box.size.x * 0.5f;
		const float halfHeight = box.size.y * 0.5f;

		Aabb2D aabb;
		aabb.minX = centerX - halfWidth;
		aabb.maxX = centerX + halfWidth;
		aabb.minY = centerY - halfHeight;
		aabb.maxY = centerY + halfHeight;
		return aabb;
	}

	/// <summary>
	/// 被弾反応で必要な Player Component をまとめて取得する。
	/// </summary>
	/// <param name="world">Component を取得する World。</param>
	/// <param name="objectId">対象 Player の GameObject ID。</param>
	/// <param name="outRuntime">取得結果を書き込む Runtime 情報。</param>
	/// <returns>必要 Component が揃っていれば true。</returns>
	bool TryBuildPlayerReactionRuntime(World& world, GameObjectId objectId, PlayerReactionRuntime& outRuntime)
	{
		GameObject* object = world.GetGameObject(objectId);
		if (!object || object->tag != GameObjectTag::Player)
		{
			return false;
		}

		TransformComponent* transform = world.GetTransform(objectId);
		StateComponent* state = world.GetComponent<StateComponent>(objectId);
		HitBoxComponent* hitBox = world.GetComponent<HitBoxComponent>(objectId);
		VelocityComponent* velocity = world.GetComponent<VelocityComponent>(objectId);
		if (!transform || !state || !hitBox || !velocity || !hitBox->pushBox.enabled)
		{
			return false;
		}

		outRuntime.objectId = objectId;
		outRuntime.transform = transform;
		outRuntime.state = state;
		outRuntime.hitBox = hitBox;
		outRuntime.velocity = velocity;
		return true;
	}

	/// <summary>
	/// PushBox がステージ左右端を越えない範囲で Player を X 方向へ移動する。
	/// </summary>
	/// <param name="player">移動対象の Player 情報。</param>
	/// <param name="requestedDeltaX">要求された X 移動量。</param>
	/// <returns>実際に移動できた X 移動量。</returns>
	float MovePlayerWithinWalls(const PlayerReactionRuntime& player, float requestedDeltaX)
	{
		if (std::fabs(requestedDeltaX) <= Epsilon)
		{
			return 0.0f;
		}

		const Aabb2D currentAabb = BuildAabb(*player.transform, *player.state, player.hitBox->pushBox);
		float allowedDeltaX = requestedDeltaX;

		const float proposedMinX = currentAabb.minX + allowedDeltaX;
		const float proposedMaxX = currentAabb.maxX + allowedDeltaX;
		if (proposedMinX < EmbedResolveSystem::StageMinX)
		{
			allowedDeltaX += EmbedResolveSystem::StageMinX - proposedMinX;
		}
		if (proposedMaxX > EmbedResolveSystem::StageMaxX)
		{
			allowedDeltaX -= proposedMaxX - EmbedResolveSystem::StageMaxX;
		}

		if (std::fabs(allowedDeltaX) <= Epsilon)
		{
			return 0.0f;
		}

		Vector3 position = TransformSystem::GetLocalPosition(*player.transform);
		position.x += allowedDeltaX;
		TransformSystem::SetLocalPosition(*player.transform, position);
		return allowedDeltaX;
	}

	/// <summary>
	/// 攻撃側から防御側へ向かう X 方向を取得する。
	/// </summary>
	/// <param name="attacker">攻撃側 Player 情報。</param>
	/// <param name="defender">防御側 Player 情報。</param>
	/// <returns>防御側が右へ離れるなら 1、左へ離れるなら -1。</returns>
	float GetAwayDirectionX(const PlayerReactionRuntime& attacker, const PlayerReactionRuntime& defender)
	{
		const float attackerX = TransformSystem::GetLocalPosition(*attacker.transform).x;
		const float defenderX = TransformSystem::GetLocalPosition(*defender.transform).x;
		if (std::fabs(defenderX - attackerX) > Epsilon)
		{
			return defenderX > attackerX ? 1.0f : -1.0f;
		}

		// 同座標に重なった場合だけ、向きの反対側を後ろ方向として使う。
		return defender.state->facingDirection == FacingDirection::Right ? -1.0f : 1.0f;
	}

	/// <summary>
	/// 防御側の実行中攻撃情報を消す。
	/// </summary>
	/// <param name="hitBox">現在攻撃情報を持つ HitBoxComponent。</param>
	void ClearCurrentAttack(HitBoxComponent& hitBox)
	{
		hitBox.currentAttack.slotId.clear();
		hitBox.currentAttack.hasHit = false;
	}
}

void HitReactionSystem::Update(World& world)
{
	const std::vector<HitReactionRequest> requests = world.GetHitReactionRequests();
	for (const HitReactionRequest& request : requests)
	{
		ApplyReactionRequest(world, request);
	}

	world.ClearHitReactionRequests();
	ResolveLandedAirHitstun(world);
}

/// <summary>
/// 1 件の被弾反応リクエストを、地上バック、ダウン、空中吹き飛びのいずれかへ変換する。
/// </summary>
/// <param name="world">対象 Component を取得する World。</param>
/// <param name="request">HitResolveSystem が積んだ被弾反応リクエスト。</param>
void HitReactionSystem::ApplyReactionRequest(World& world, const HitReactionRequest& request)
{
	const HitReactionSetting setting = GetReactionSetting(request.hitReactionType);
	if (request.guarded)
	{
		ApplyNormalBack(world, request, setting.groundBackDistance);
		return;
	}

	if (!request.defenderWasGrounded)
	{
		ApplyAirBurst(world, request, GetAirFollowupVelocity());
		return;
	}

	switch (request.hitReactionType)
	{
	case HitReactionType::Down:
		ApplyDown(world, request.defenderId, setting.downFrames);
		break;
	case HitReactionType::Burst:
	case HitReactionType::HardBurst:
		ApplyAirBurst(world, request, setting.airVelocity);
		break;
	case HitReactionType::Normal:
	case HitReactionType::Unknown:
	default:
		ApplyNormalBack(world, request, setting.groundBackDistance);
		break;
	}
}

/// <summary>
/// 防御側を後ろへ即時移動し、壁で下がれなかった不足分を攻撃側へ返す。
/// </summary>
/// <param name="world">対象 Component を取得する World。</param>
/// <param name="request">攻撃側と防御側の GameObject ID を持つリクエスト。</param>
/// <param name="backDistance">このヒット/ガードで確保したい距離。</param>
void HitReactionSystem::ApplyNormalBack(World& world, const HitReactionRequest& request, float backDistance)
{
	if (backDistance <= 0.0f)
	{
		return;
	}

	PlayerReactionRuntime attacker;
	PlayerReactionRuntime defender;
	if (!TryBuildPlayerReactionRuntime(world, request.attackerId, attacker)
		|| !TryBuildPlayerReactionRuntime(world, request.defenderId, defender))
	{
		return;
	}

	const float awayDirectionX = GetAwayDirectionX(attacker, defender);
	const float requestedDefenderDeltaX = awayDirectionX * backDistance;
	const float movedDefenderDeltaX = MovePlayerWithinWalls(defender, requestedDefenderDeltaX);
	const float remainingDeltaX = requestedDefenderDeltaX - movedDefenderDeltaX;

	if (std::fabs(remainingDeltaX) > Epsilon)
	{
		MovePlayerWithinWalls(attacker, -remainingDeltaX);
	}
}

/// <summary>
/// 防御側をその場ダウンへ遷移させ、移動速度と攻撃情報を消す。
/// </summary>
/// <param name="world">対象 Component を取得する World。</param>
/// <param name="defenderId">ダウンさせる Player GameObject ID。</param>
/// <param name="downFrames">Down 状態を維持するフレーム数。</param>
void HitReactionSystem::ApplyDown(World& world, GameObjectId defenderId, int downFrames)
{
	PlayerReactionRuntime defender;
	if (!TryBuildPlayerReactionRuntime(world, defenderId, defender))
	{
		return;
	}

	defender.state->currentActionState = PlayerActionState::Down;
	defender.state->actionFrame = 0;
	defender.state->actionDurationFrames = downFrames;
	defender.state->isGrounded = true;
	defender.state->hitstunRequested = false;
	defender.state->cancelEnabled = false;
	defender.velocity->velocity = Vector3::Zero;
	ClearCurrentAttack(*defender.hitBox);
}

/// <summary>
/// 防御側を空中被弾へ遷移させ、指定された吹き飛び速度を設定する。
/// </summary>
/// <param name="world">対象 Component を取得する World。</param>
/// <param name="request">攻撃側と防御側の GameObject ID を持つリクエスト。</param>
/// <param name="baseVelocity">X は正値で受け取り、攻撃側から離れる向きに反転して使う速度。</param>
void HitReactionSystem::ApplyAirBurst(
	World& world,
	const HitReactionRequest& request,
	const Vector3& baseVelocity)
{
	PlayerReactionRuntime attacker;
	PlayerReactionRuntime defender;
	if (!TryBuildPlayerReactionRuntime(world, request.attackerId, attacker)
		|| !TryBuildPlayerReactionRuntime(world, request.defenderId, defender))
	{
		return;
	}

	const float awayDirectionX = GetAwayDirectionX(attacker, defender);
	defender.state->currentActionState = PlayerActionState::AirHitstun;
	defender.state->actionFrame = 0;
	defender.state->actionDurationFrames = 0;
	defender.state->isGrounded = false;
	defender.state->hitstunRequested = false;
	defender.state->cancelEnabled = false;
	defender.velocity->velocity = Vector3(baseVelocity.x * awayDirectionX, baseVelocity.y, baseVelocity.z);
	ClearCurrentAttack(*defender.hitBox);
}

/// <summary>
/// AirHitstun の Player が地面に着いていれば、そのフレーム終端で Down へ遷移させる。
/// </summary>
/// <param name="world">対象 Component を取得する World。</param>
void HitReactionSystem::ResolveLandedAirHitstun(World& world)
{
	for (GameObject& object : world.GetGameObjects())
	{
		if (object.tag != GameObjectTag::Player)
		{
			continue;
		}

		StateComponent* state = world.GetComponent<StateComponent>(object.id);
		if (!state
			|| state->currentActionState != PlayerActionState::AirHitstun
			|| !state->isGrounded)
		{
			continue;
		}

		ApplyDown(world, object.id, DefaultDownFrames);
	}
}
