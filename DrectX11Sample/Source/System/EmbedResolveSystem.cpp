#include "System/EmbedResolveSystem.h"

#include "Component/HitBoxComponent.h"
#include "Component/StateComponent.h"
#include "Component/TransformComponent.h"
#include "Component/VelocityComponent.h"
#include "System/TransformSystem.h"
#include "World/World.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace DirectX::SimpleMath;

namespace
{
	constexpr float StageMinX = -15.0f;
	constexpr float StageMaxX = 15.0f;
	constexpr float Epsilon = 0.0001f;

	struct Aabb2D
	{
		float minX = 0.0f;
		float maxX = 0.0f;
		float minY = 0.0f;
		float maxY = 0.0f;
	};

	struct PlayerPushBoxRuntime
	{
		GameObjectId objectId = INVALID_GAME_OBJECT_ID;
		TransformComponent* transform = nullptr;
		StateComponent* state = nullptr;
		HitBoxComponent* hitBox = nullptr;
	};

	/// <summary>
	/// Transform と FacingDirection から、指定 HitBox のワールド上 AABB を作る。
	/// </summary>
	/// <param name="transform">基準座標を持つ TransformComponent。</param>
	/// <param name="state">左右反転に使う StateComponent。</param>
	/// <param name="box">AABB 化する HitBoxRect2D。</param>
	/// <returns>X/Y 平面上で扱う AABB。</returns>
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
	/// 2 つの AABB の X 方向めり込み量を計算する。
	/// </summary>
	/// <param name="a">比較する AABB。</param>
	/// <param name="b">比較する AABB。</param>
	/// <returns>X 方向の重なり量。重なっていない場合は 0 以下。</returns>
	float GetOverlapX(const Aabb2D& a, const Aabb2D& b)
	{
		return std::min(a.maxX, b.maxX) - std::max(a.minX, b.minX);
	}

	/// <summary>
	/// AABB の中心 X 座標を取得する。
	/// </summary>
	/// <param name="aabb">中心を求める AABB。</param>
	/// <returns>AABB の中心 X 座標。</returns>
	float GetCenterX(const Aabb2D& aabb)
	{
		return (aabb.minX + aabb.maxX) * 0.5f;
	}

	/// <summary>
	/// Player の PushBox 補正に必要な Component を取得する。
	/// </summary>
	/// <param name="world">Component を取得する World。</param>
	/// <param name="objectId">対象 GameObject ID。</param>
	/// <param name="outRuntime">取得した Component の書き込み先。</param>
	/// <returns>PushBox 補正に必要な Component が揃っていれば true。</returns>
	bool TryBuildPlayerPushBoxRuntime(World& world, GameObjectId objectId, PlayerPushBoxRuntime& outRuntime)
	{
		GameObject* object = world.GetGameObject(objectId);
		if (!object || object->tag != GameObjectTag::Player)
		{
			return false;
		}

		TransformComponent* transform = world.GetTransform(objectId);
		StateComponent* state = world.GetComponent<StateComponent>(objectId);
		HitBoxComponent* hitBox = world.GetComponent<HitBoxComponent>(objectId);
		if (!transform || !state || !hitBox || !hitBox->pushBox.enabled)
		{
			return false;
		}

		outRuntime.objectId = objectId;
		outRuntime.transform = transform;
		outRuntime.state = state;
		outRuntime.hitBox = hitBox;
		return true;
	}

	/// <summary>
	/// PlayerActionState と向きから、歩き入力が作る X 方向を取得する。
	/// </summary>
	/// <param name="state">歩き状態と向きを確認する StateComponent。</param>
	/// <returns>右方向なら 1、左方向なら -1、歩きでなければ 0。</returns>
	float GetWalkDirectionX(const StateComponent& state)
	{
		const float facingSign = state.facingDirection == FacingDirection::Right ? 1.0f : -1.0f;

		if (state.currentActionState == PlayerActionState::FrontWalk)
		{
			return facingSign;
		}

		if (state.currentActionState == PlayerActionState::BackWalk)
		{
			return -facingSign;
		}

		return 0.0f;
	}

	/// <summary>
	/// 指定方向の歩きが相手方向へ向かっているか判定する。
	/// </summary>
	/// <param name="walkDirectionX">歩きで発生している X 方向。</param>
	/// <param name="directionToOpponent">自分から相手へ向かう X 方向。</param>
	/// <returns>相手側へ歩いていれば true。</returns>
	bool IsWalkingIntoOpponent(float walkDirectionX, float directionToOpponent)
	{
		return walkDirectionX * directionToOpponent > Epsilon;
	}

	/// <summary>
	/// PushBox がステージ壁を越えない範囲で Transform を X 方向へ移動する。
	/// </summary>
	/// <param name="player">移動対象の Player 情報。</param>
	/// <param name="requestedDeltaX">要求された X 移動量。</param>
	/// <returns>実際に移動できた X 移動量。</returns>
	float MovePlayerWithinWalls(const PlayerPushBoxRuntime& player, float requestedDeltaX)
	{
		if (std::fabs(requestedDeltaX) <= Epsilon)
		{
			return 0.0f;
		}

		const Aabb2D currentAabb = BuildAabb(*player.transform, *player.state, player.hitBox->pushBox);
		float allowedDeltaX = requestedDeltaX;

		const float proposedMinX = currentAabb.minX + allowedDeltaX;
		const float proposedMaxX = currentAabb.maxX + allowedDeltaX;
		if (proposedMinX < StageMinX)
		{
			allowedDeltaX += StageMinX - proposedMinX;
		}
		if (proposedMaxX > StageMaxX)
		{
			allowedDeltaX -= proposedMaxX - StageMaxX;
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
	/// 片方を優先して押し、壁で押し切れなかった残りを押した側へ戻す。
	/// </summary>
	/// <param name="primary">先に動かす Player。</param>
	/// <param name="secondary">残り分を受ける Player。</param>
	/// <param name="primaryDeltaX">primary に要求する X 移動量。</param>
	void MovePrimaryThenSecondary(const PlayerPushBoxRuntime& primary, const PlayerPushBoxRuntime& secondary, float primaryDeltaX)
	{
		const float movedPrimary = MovePlayerWithinWalls(primary, primaryDeltaX);
		const float remainingDeltaX = primaryDeltaX - movedPrimary;
		if (std::fabs(remainingDeltaX) > Epsilon)
		{
			MovePlayerWithinWalls(secondary, -remainingDeltaX);
		}
	}

	/// <summary>
	/// 両方が押し合う、または押している側が明確でないめり込みを半分ずつ解消する。
	/// </summary>
	/// <param name="a">片方の Player。</param>
	/// <param name="b">もう片方の Player。</param>
	/// <param name="directionFromAToB">A から B へ向かう X 方向。</param>
	/// <param name="overlapX">解消する X 方向めり込み量。</param>
	void SplitResolveOverlap(
		const PlayerPushBoxRuntime& a,
		const PlayerPushBoxRuntime& b,
		float directionFromAToB,
		float overlapX)
	{
		const float desiredA = -directionFromAToB * overlapX * 0.5f;
		const float desiredB = directionFromAToB * overlapX * 0.5f;

		const float movedA = MovePlayerWithinWalls(a, desiredA);
		const float movedB = MovePlayerWithinWalls(b, desiredB);

		const float remainingA = desiredA - movedA;
		const float remainingB = desiredB - movedB;
		if (std::fabs(remainingA) > Epsilon)
		{
			MovePlayerWithinWalls(b, -remainingA);
		}
		if (std::fabs(remainingB) > Epsilon)
		{
			MovePlayerWithinWalls(a, -remainingB);
		}
	}

	/// <summary>
	/// 2 人の Player の PushBox めり込みを横方向に解消する。
	/// </summary>
	/// <param name="a">片方の Player。</param>
	/// <param name="b">もう片方の Player。</param>
	void ResolvePlayerPair(const PlayerPushBoxRuntime& a, const PlayerPushBoxRuntime& b)
	{
		const Aabb2D aabbA = BuildAabb(*a.transform, *a.state, a.hitBox->pushBox);
		const Aabb2D aabbB = BuildAabb(*b.transform, *b.state, b.hitBox->pushBox);
		if (!IsOverlapping(aabbA, aabbB))
		{
			return;
		}

		const float overlapX = GetOverlapX(aabbA, aabbB);
		if (overlapX <= Epsilon)
		{
			return;
		}

		const float directionFromAToB = GetCenterX(aabbA) <= GetCenterX(aabbB) ? 1.0f : -1.0f;
		const bool aPushesB = IsWalkingIntoOpponent(GetWalkDirectionX(*a.state), directionFromAToB);
		const bool bPushesA = IsWalkingIntoOpponent(GetWalkDirectionX(*b.state), -directionFromAToB);

		if (aPushesB && !bPushesA)
		{
			// A だけが B 方向へ歩いてめり込んだ場合、歩いていない側の B を押す。
			MovePrimaryThenSecondary(b, a, directionFromAToB * overlapX);
			return;
		}

		if (bPushesA && !aPushesB)
		{
			// B だけが A 方向へ歩いてめり込んだ場合、歩いていない側の A を押す。
			MovePrimaryThenSecondary(a, b, -directionFromAToB * overlapX);
			return;
		}

		// 両方が押し合う場合や、どちらが押したか確定できない場合は互いに離す。
		SplitResolveOverlap(a, b, directionFromAToB, overlapX);
	}
}

void EmbedResolveSystem::Update(World& world)
{
	ResolveWallBounds(world);
	ResolvePlayerPushBoxes(world);
	ResolveWallBounds(world);
	ResolveTemporaryGround(world);
}

/// <summary>
/// Player の PushBox がステージ左右端を越えないよう補正する。
/// </summary>
/// <param name="world">補正対象の Component を取得する World。</param>
void EmbedResolveSystem::ResolveWallBounds(World& world)
{
	for (GameObject& object : world.GetGameObjects())
	{
		PlayerPushBoxRuntime player;
		if (!TryBuildPlayerPushBoxRuntime(world, object.id, player))
		{
			continue;
		}

		const Aabb2D aabb = BuildAabb(*player.transform, *player.state, player.hitBox->pushBox);
		if (aabb.minX < StageMinX)
		{
			MovePlayerWithinWalls(player, StageMinX - aabb.minX);
		}
		else if (aabb.maxX > StageMaxX)
		{
			MovePlayerWithinWalls(player, StageMaxX - aabb.maxX);
		}
	}
}

/// <summary>
/// Player 同士の PushBox めり込みを横方向に解消する。
/// </summary>
/// <param name="world">補正対象の Component を取得する World。</param>
void EmbedResolveSystem::ResolvePlayerPushBoxes(World& world)
{
	std::vector<GameObjectId> playerIds;
	for (const GameObject& object : world.GetGameObjects())
	{
		if (object.tag == GameObjectTag::Player)
		{
			playerIds.push_back(object.id);
		}
	}

	for (size_t i = 0; i < playerIds.size(); ++i)
	{
		for (size_t j = i + 1; j < playerIds.size(); ++j)
		{
			PlayerPushBoxRuntime a;
			PlayerPushBoxRuntime b;
			if (!TryBuildPlayerPushBoxRuntime(world, playerIds[i], a)
				|| !TryBuildPlayerPushBoxRuntime(world, playerIds[j], b))
			{
				continue;
			}

			ResolvePlayerPair(a, b);
		}
	}
}

/// <summary>
/// 仮の地面判定として y <= 0 を接地扱いに補正する。
/// </summary>
/// <param name="world">接地補正対象の Component を取得する World。</param>
void EmbedResolveSystem::ResolveTemporaryGround(World& world)
{
	for (GameObject& object : world.GetGameObjects())
	{
		TransformComponent* transform = world.GetTransform(object.id);
		StateComponent* state = world.GetComponent<StateComponent>(object.id);
		VelocityComponent* velocity = world.GetComponent<VelocityComponent>(object.id);
		if (!transform || !state || !velocity)
		{
			continue;
		}

		Vector3 position = TransformSystem::GetLocalPosition(*transform);
		if (position.y <= 0.0f)
		{
			position.y = 0.0f;
			TransformSystem::SetLocalPosition(*transform, position);
			velocity->velocity.y = 0.0f;
			state->isGrounded = true;
			continue;
		}

		state->isGrounded = false;
	}
}
