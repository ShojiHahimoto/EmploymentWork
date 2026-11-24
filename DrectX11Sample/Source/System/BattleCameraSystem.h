#pragma once

#include "Component/CameraComponent.h"
#include "Component/TransformComponent.h"

class World;

class BattleCameraSystem
{
public:
	static void Update(World& world);

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
