#include "System/BattleCameraSystem.h"

#include "Component/BattleCameraFollowComponent.h"
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
	constexpr float MinProjectionDistance = 0.001f;

	struct BattlePlayerCameraTarget
	{
		Vector3 position = Vector3::Zero;
		const StateComponent* state = nullptr;
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
		if (!transform || !state)
		{
			return false;
		}

		outTarget.position = TransformSystem::GetLocalPosition(*transform);
		outTarget.state = state;
		return true;
	}

	/// <summary>
	/// 値を 1 フレームで動かせる最大量以内で target へ近づける。
	/// </summary>
	/// <param name="current">現在値。</param>
	/// <param name="target">目標値。</param>
	/// <param name="maxDelta">このフレームで変化できる最大量。</param>
	/// <returns>target に近づけた値。</returns>
	float MoveTowards(float current, float target, float maxDelta)
	{
		const float delta = target - current;
		const float absDelta = std::fabs(delta);
		if (absDelta <= maxDelta)
		{
			return target;
		}

		return current + (delta > 0.0f ? maxDelta : -maxDelta);
	}

	/// <summary>
	/// 2 Player 中心へ向けて、加速と減速を使った次のカメラ X 座標を計算する。
	/// </summary>
	/// <param name="currentCameraX">現在のカメラ X 座標。</param>
	/// <param name="targetCameraX">常に 2 Player 中心から求める目標カメラ X 座標。</param>
	/// <param name="follow">カメラの追従速度と調整値を保持する Component。</param>
	/// <returns>このフレームで反映するカメラ X 座標。</returns>
	float CalculateSmoothCameraX(
		float currentCameraX,
		float targetCameraX,
		BattleCameraFollowComponent& follow)
	{
		const float distance = targetCameraX - currentCameraX;
		if (std::fabs(distance) <= follow.snapDistance)
		{
			follow.velocityX = 0.0f;
			return targetCameraX;
		}

		const float desiredVelocity = std::clamp(
			distance * follow.followPower,
			-follow.maxSpeed,
			follow.maxSpeed);
		const bool changingDirection = follow.velocityX * desiredVelocity < 0.0f;
		const bool reducingSpeed = std::fabs(desiredVelocity) < std::fabs(follow.velocityX);
		const float maxVelocityDelta = (changingDirection || reducingSpeed)
			? follow.deceleration
			: follow.acceleration;

		follow.velocityX = MoveTowards(follow.velocityX, desiredVelocity, maxVelocityDelta);
		if (std::fabs(follow.velocityX) <= follow.stopSpeed && std::fabs(desiredVelocity) <= follow.stopSpeed)
		{
			follow.velocityX = 0.0f;
		}

		const float nextCameraX = currentCameraX + follow.velocityX;
		const float nextDistance = targetCameraX - nextCameraX;
		if (distance * nextDistance <= 0.0f)
		{
			follow.velocityX = 0.0f;
			return targetCameraX;
		}

		return nextCameraX;
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
	const GameObjectId cameraId = world.GetActiveCameraId();
	TransformComponent* cameraTransform = world.GetTransform(cameraId);
	BattleCameraFollowComponent* follow = world.GetComponent<BattleCameraFollowComponent>(cameraId);
	if (!cameraTransform || !follow)
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
	const float targetCameraX = ClampCameraXToStage(playerCenterX, visibleHalfWidth);
	const float smoothCameraX = CalculateSmoothCameraX(cameraPosition.x, targetCameraX, *follow);
	const float clampedCameraX = ClampCameraXToStage(smoothCameraX, visibleHalfWidth);
	if (clampedCameraX != smoothCameraX)
	{
		follow->velocityX = 0.0f;
	}

	const float highestPlayerY = CalculateHighestNaturalJumpY(player1, player2);
	const float targetCameraY = BaseCameraY + std::clamp(highestPlayerY * JumpLiftRate, 0.0f, MaxJumpLift);

	cameraPosition.x = clampedCameraX;
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
