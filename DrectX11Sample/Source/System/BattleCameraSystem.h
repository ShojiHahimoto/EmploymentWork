#pragma once

#include "Component/CameraComponent.h"
#include "Component/TransformComponent.h"

class World;

class BattleCameraSystem
{
public:
	/// <summary>
	/// 2 Player の位置から、バトル用メインカメラの Transform を更新する。
	/// </summary>
	/// <param name="world">ActiveCamera と BattlePlayerId を保持する World。</param>
	static void Update(World& world);

	/// <summary>
	/// 指定 Z 平面上で、現在のカメラに映る X 範囲を計算する。
	/// </summary>
	/// <param name="camera">FOV とアスペクト比を持つ CameraComponent。</param>
	/// <param name="cameraTransform">カメラ位置を持つ TransformComponent。</param>
	/// <param name="targetPlaneZ">表示範囲を求めたい対象平面の Z 座標。</param>
	/// <param name="outMinX">表示可能な最小 X の書き込み先。</param>
	/// <param name="outMaxX">表示可能な最大 X の書き込み先。</param>
	/// <returns>有効な表示範囲を計算できた場合は true。</returns>
	static bool CalculateVisibleXRange(
		const CameraComponent& camera,
		const TransformComponent& cameraTransform,
		float targetPlaneZ,
		float& outMinX,
		float& outMaxX);

private:
	static float CalculateVisibleHalfWidth(
		const CameraComponent& camera,
		const TransformComponent& cameraTransform,
		float targetPlaneZ);
	static float ClampCameraXToStage(float targetCameraX, float visibleHalfWidth);
};
