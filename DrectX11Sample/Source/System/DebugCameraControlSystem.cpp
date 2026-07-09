#include "System/DebugCameraControlSystem.h"

#include "System/Application.h"
#include "System/TransformSystem.h"

#if defined(_DEBUG)
#include <Windows.h>
#include <DirectXMath.h>
#endif

using namespace DirectX;
using namespace DirectX::SimpleMath;

#if defined(_DEBUG)
namespace
{
	constexpr float MouseSensitivityDegrees = 0.15f;
	constexpr float MoveStepPerFrame = 0.12f;
	constexpr float MinPitchDegrees = -89.0f;
	constexpr float MaxPitchDegrees = 89.0f;

	bool IsKeyDown(int virtualKey)
	{
		return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
	}

	float Clamp(float value, float minValue, float maxValue)
	{
		if (value < minValue)
		{
			return minValue;
		}

		if (value > maxValue)
		{
			return maxValue;
		}

		return value;
	}

	Vector3 RotateDirection(const Vector3& direction, const Quaternion& rotation)
	{
		XMVECTOR rotated = XMVector3Rotate(XMLoadFloat3(&direction), XMLoadFloat4(&rotation));

		Vector3 result;
		XMStoreFloat3(&result, rotated);
		result.Normalize();
		return result;
	}
}
#endif

void DebugCameraControlSystem::Update(TransformComponent& cameraTransform, DebugCameraControlState& state)
{
#if defined(_DEBUG)
	if (GetForegroundWindow() != Application::GetWindow() || !IsKeyDown(VK_RBUTTON))
	{
		state.hasPreviousMousePosition = false;
		return;
	}

	POINT mousePosition = {};
	if (!GetCursorPos(&mousePosition))
	{
		return;
	}

	if (!state.hasPreviousMousePosition)
	{
		state.previousMouseX = mousePosition.x;
		state.previousMouseY = mousePosition.y;
		state.hasPreviousMousePosition = true;
	}

	const int deltaX = mousePosition.x - state.previousMouseX;
	const int deltaY = mousePosition.y - state.previousMouseY;
	state.previousMouseX = mousePosition.x;
	state.previousMouseY = mousePosition.y;

	state.yawDegrees += static_cast<float>(deltaX) * MouseSensitivityDegrees;
	state.pitchDegrees += static_cast<float>(deltaY) * MouseSensitivityDegrees;
	state.pitchDegrees = Clamp(state.pitchDegrees, MinPitchDegrees, MaxPitchDegrees);

	TransformSystem::SetLocalEulerRotationDegrees(
		cameraTransform,
		Vector3(state.pitchDegrees, state.yawDegrees, 0.0f));

	const Quaternion rotation = cameraTransform.localRotation;
	const Vector3 forward = RotateDirection(Vector3(0.0f, 0.0f, 1.0f), rotation);
	const Vector3 right = RotateDirection(Vector3(1.0f, 0.0f, 0.0f), rotation);
	const Vector3 up = Vector3(0.0f, 1.0f, 0.0f);

	Vector3 move = Vector3::Zero;
	if (IsKeyDown('W')) move += forward;
	if (IsKeyDown('S')) move -= forward;
	if (IsKeyDown('D')) move += right;
	if (IsKeyDown('A')) move -= right;
	if (IsKeyDown('E')) move += up;
	if (IsKeyDown('Q')) move -= up;

	if (move.LengthSquared() > 0.0f)
	{
		move.Normalize();
		TransformSystem::SetLocalPosition(
			cameraTransform,
			cameraTransform.localPosition + move * MoveStepPerFrame);
	}
#else
	(void)cameraTransform;
	(void)state;
#endif
}
