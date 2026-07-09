#pragma once

#include "Component/TransformComponent.h"

struct DebugCameraControlState
{
	float yawDegrees = 0.0f;
	float pitchDegrees = 0.0f;
	bool hasPreviousMousePosition = false;
	int previousMouseX = 0;
	int previousMouseY = 0;
};

class DebugCameraControlSystem
{
public:
	static void Update(TransformComponent& cameraTransform, DebugCameraControlState& state);
};
