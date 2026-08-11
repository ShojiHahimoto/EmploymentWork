#include "System/BattleCameraSystem.h"

#include "Component/HitBoxComponent.h"
#include "Component/StateComponent.h"
#include "System/EmbedResolveSystem.h"
#include "System/TransformSystem.h"
#include "World/World.h"

#include <DirectXMath.h>

#include <algorithm>
#include <cmath>

using namespace DirectX;
using namespace DirectX::SimpleMath;

namespace
{
	constexpr float BaseCameraY = 8.0f;
	constexpr float BaseCameraZ = -20.0f;
	constexpr float JumpLiftRate = 0.22f;
	constexpr float MaxJumpLift = 2.0f;
	constexpr float VerticalFollowRate = 0.10f;
	constexpr float CenterDeadZoneHalfWidth = 2.0f;
	constexpr float CameraEdgeFollowMargin = 2.0f;
	constexpr float MinProjectionDistance = 0.001f;

	struct BattlePlayerCameraTarget
	{
		Vector3 position = Vector3::Zero;
		const StateComponent* state = nullptr;
		const HitBoxComponent* hitBox = nullptr;
	};

	struct Aabb2D
	{
		float minX = 0.0f;
		float maxX = 0.0f;
	};

	/// <summary>
	/// 指定 Player の Transform と State を取得する。
	/// </summary>
	/// <param name="world">Player の Transform / State を保持する World。</param>
	/// <param name="playerIndex">World の BattlePlayerId に対応する Player 番号。</param>
	/// <param name="outTarget">取得したカメラ追従用情報の書き込み先。</param>
	/// <returns>Player と必要 Component が存在する場合は true。</returns>
	bool TryGetBattlePlayerCameraTarget(World& world, int playerIndex, BattlePlayerCameraTarget& outTarget)
	{
		const GameObjectId playerId = world.GetBattlePlayerId(playerIndex);
		const TransformComponent* transform = world.GetTransform(playerId);
		const StateComponent* state = world.GetComponent<StateComponent>(playerId);
		const HitBoxComponent* hitBox = world.GetComponent<HitBoxComponent>(playerId);
		if (!transform || !state)
		{
			return false;
		}

		outTarget.position = TransformSystem::GetLocalPosition(*transform);
		outTarget.state = state;
		outTarget.hitBox = hitBox;
		return true;
	}

	/// <summary>
	/// Player の PushBox から、カメラ追従判定用の X 範囲を作る。
	/// </summary>
	/// <param name="player">Player の位置、向き、HitBox を持つカメラ追従情報。</param>
	/// <returns>PushBox があればその X 範囲。なければ Player 位置だけの X 範囲。</returns>
	Aabb2D BuildPlayerCameraAabb(const BattlePlayerCameraTarget& player)
	{
		if (!player.hitBox || !player.hitBox->pushBox.enabled)
		{
			return { player.position.x, player.position.x };
		}

		const float facingSign = player.state->facingDirection == FacingDirection::Right ? 1.0f : -1.0f;
		const float centerX = player.position.x + player.hitBox->pushBox.offset.x * facingSign;
		const float halfWidth = player.hitBox->pushBox.size.x * 0.5f;
		return { centerX - halfWidth, centerX + halfWidth };
	}

	/// <summary>
	/// カメラ中心から左右に同じ幅のデッドゾーンを置き、2 Player 中心が外に出た分だけ追従する。
	/// </summary>
	/// <param name="currentCameraX">現在のカメラ X 座標。</param>
	/// <param name="playerCenterX">2 Player の中心 X 座標。</param>
	/// <returns>中心デッドゾーンを超えた分だけ追従したカメラ X 座標。</returns>
	float CalculateCenterDeadZoneCameraX(float currentCameraX, float playerCenterX)
	{
		const float deadZoneMinX = currentCameraX - CenterDeadZoneHalfWidth;
		const float deadZoneMaxX = currentCameraX + CenterDeadZoneHalfWidth;

		if (playerCenterX < deadZoneMinX)
		{
			return currentCameraX + (playerCenterX - deadZoneMinX);
		}
		if (playerCenterX > deadZoneMaxX)
		{
			return currentCameraX + (playerCenterX - deadZoneMaxX);
		}

		return currentCameraX;
	}

	/// <summary>
	/// Player が画面端へ近づいた場合、位置補正で止める前にカメラを動かせるだけ動かす。
	/// </summary>
	/// <param name="targetCameraX">中心デッドゾーンから求めた仮カメラ X 座標。</param>
	/// <param name="visibleHalfWidth">対象平面上で画面半分に相当する X 幅。</param>
	/// <param name="player1">Player1 のカメラ追従情報。</param>
	/// <param name="player2">Player2 のカメラ追従情報。</param>
	/// <returns>Player が端へ寄った分を加味したカメラ X 座標。</returns>
	float AdjustCameraXForPlayerScreenEdges(
		float targetCameraX,
		float visibleHalfWidth,
		const BattlePlayerCameraTarget& player1,
		const BattlePlayerCameraTarget& player2)
	{
		const Aabb2D player1Aabb = BuildPlayerCameraAabb(player1);
		const Aabb2D player2Aabb = BuildPlayerCameraAabb(player2);
		const float playersMinX = std::min(player1Aabb.minX, player2Aabb.minX);
		const float playersMaxX = std::max(player1Aabb.maxX, player2Aabb.maxX);
		const float safeVisibleMinX = targetCameraX - visibleHalfWidth + CameraEdgeFollowMargin;
		const float safeVisibleMaxX = targetCameraX + visibleHalfWidth - CameraEdgeFollowMargin;

		if (playersMaxX - playersMinX > safeVisibleMaxX - safeVisibleMinX)
		{
			return (playersMinX + playersMaxX) * 0.5f;
		}

		if (playersMinX < safeVisibleMinX)
		{
			return targetCameraX + (playersMinX - safeVisibleMinX);
		}
		if (playersMaxX > safeVisibleMaxX)
		{
			return targetCameraX + (playersMaxX - safeVisibleMaxX);
		}

		return targetCameraX;
	}

	/// <summary>
	/// 通常ジャンプ由来としてカメラが追ってよい最大 Y 座標を取得する。
	/// </summary>
	/// <param name="player1">Player1 のカメラ追従用情報。</param>
	/// <param name="player2">Player2 のカメラ追従用情報。</param>
	/// <returns>追従対象がいればその最大 Y。いなければ 0。</returns>
	float CalculateHighestNaturalJumpY(
		const BattlePlayerCameraTarget& player1,
		const BattlePlayerCameraTarget& player2)
	{
		float highestY = 0.0f;
		if (player1.state->cameraYFollowMode == CameraYFollowMode::NaturalJump)
		{
			highestY = std::max(highestY, player1.position.y);
		}
		if (player2.state->cameraYFollowMode == CameraYFollowMode::NaturalJump)
		{
			highestY = std::max(highestY, player2.position.y);
		}

		return highestY;
	}
}

void BattleCameraSystem::Update(World& world)
{
	if (!world.HasActiveCamera())
	{
		return;
	}

	CameraComponent& camera = world.GetActiveCamera();
	TransformComponent* cameraTransform = world.GetTransform(world.GetActiveCameraId());
	if (!cameraTransform)
	{
		return;
	}

	BattlePlayerCameraTarget player1;
	BattlePlayerCameraTarget player2;
	if (!TryGetBattlePlayerCameraTarget(world, 0, player1)
		|| !TryGetBattlePlayerCameraTarget(world, 1, player2))
	{
		return;
	}

	Vector3 cameraPosition = TransformSystem::GetLocalPosition(*cameraTransform);
	const float targetPlaneZ = (player1.position.z + player2.position.z) * 0.5f;
	const float visibleHalfWidth = CalculateVisibleHalfWidth(camera, *cameraTransform, targetPlaneZ);
	const float playerCenterX = (player1.position.x + player2.position.x) * 0.5f;
	const float deadZoneCameraX = CalculateCenterDeadZoneCameraX(cameraPosition.x, playerCenterX);
	const float edgeAdjustedCameraX = AdjustCameraXForPlayerScreenEdges(
		deadZoneCameraX,
		visibleHalfWidth,
		player1,
		player2);
	const float targetCameraX = ClampCameraXToStage(edgeAdjustedCameraX, visibleHalfWidth);

	const float highestPlayerY = CalculateHighestNaturalJumpY(player1, player2);
	const float targetCameraY = BaseCameraY + std::clamp(highestPlayerY * JumpLiftRate, 0.0f, MaxJumpLift);

	cameraPosition.x = targetCameraX;
	cameraPosition.y += (targetCameraY - cameraPosition.y) * VerticalFollowRate;
	cameraPosition.z = BaseCameraZ;
	TransformSystem::SetLocalPosition(*cameraTransform, cameraPosition);
}

/// <summary>
/// 指定 Z 平面上で、現在のカメラに映る X 範囲を計算する。
/// </summary>
/// <param name="camera">FOV とアスペクト比を持つ CameraComponent。</param>
/// <param name="cameraTransform">カメラ位置を持つ TransformComponent。</param>
/// <param name="targetPlaneZ">表示範囲を求めたい対象平面の Z 座標。</param>
/// <param name="outMinX">表示可能な最小 X の書き込み先。</param>
/// <param name="outMaxX">表示可能な最大 X の書き込み先。</param>
/// <returns>有効な表示範囲を計算できた場合は true。</returns>
bool BattleCameraSystem::CalculateVisibleXRange(
	const CameraComponent& camera,
	const TransformComponent& cameraTransform,
	float targetPlaneZ,
	float& outMinX,
	float& outMaxX)
{
	const float visibleHalfWidth = CalculateVisibleHalfWidth(camera, cameraTransform, targetPlaneZ);
	if (visibleHalfWidth <= 0.0f)
	{
		return false;
	}

	const Vector3 cameraPosition = TransformSystem::GetLocalPosition(cameraTransform);
	outMinX = cameraPosition.x - visibleHalfWidth;
	outMaxX = cameraPosition.x + visibleHalfWidth;
	return true;
}

/// <summary>
/// カメラの FOV、アスペクト比、対象 Z 平面までの距離から、画面半分の横幅を計算する。
/// </summary>
/// <param name="camera">FOV とアスペクト比を持つ CameraComponent。</param>
/// <param name="cameraTransform">カメラ位置を持つ TransformComponent。</param>
/// <param name="targetPlaneZ">表示範囲を求めたい対象平面の Z 座標。</param>
/// <returns>対象平面上で画面半分に相当する X 幅。</returns>
float BattleCameraSystem::CalculateVisibleHalfWidth(
	const CameraComponent& camera,
	const TransformComponent& cameraTransform,
	float targetPlaneZ)
{
	const Vector3 cameraPosition = TransformSystem::GetLocalPosition(cameraTransform);
	const float distanceToPlane = std::max(MinProjectionDistance, targetPlaneZ - cameraPosition.z);
	const float fovRadians = XMConvertToRadians(camera.fovYDegrees);
	const float visibleHalfHeight = std::tan(fovRadians * 0.5f) * distanceToPlane;
	return visibleHalfHeight * camera.aspectRatio;
}

/// <summary>
/// ステージ外が画面に映らないよう、カメラ X 座標をステージ内に丸める。
/// </summary>
/// <param name="targetCameraX">追従計算から求めたカメラ X 座標。</param>
/// <param name="visibleHalfWidth">対象平面上で画面半分に相当する X 幅。</param>
/// <returns>ステージ端を越えないカメラ X 座標。</returns>
float BattleCameraSystem::ClampCameraXToStage(float targetCameraX, float visibleHalfWidth)
{
	const float minCameraX = EmbedResolveSystem::StageMinX + visibleHalfWidth;
	const float maxCameraX = EmbedResolveSystem::StageMaxX - visibleHalfWidth;
	if (minCameraX > maxCameraX)
	{
		return (EmbedResolveSystem::StageMinX + EmbedResolveSystem::StageMaxX) * 0.5f;
	}

	return std::clamp(targetCameraX, minCameraX, maxCameraX);
}
