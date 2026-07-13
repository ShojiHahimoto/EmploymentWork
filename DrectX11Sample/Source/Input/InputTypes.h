#pragma once

#include <SimpleMath.h>

#include <array>
#include <cstdint>

namespace Input
{
	constexpr int MaxPlayers = 2;

	enum class InputActionMapId : uint8_t
	{
		Gameplay,
		UI,
		Count
	};

	enum class InputActionId : uint8_t
	{
		Move,
		Jump,
		LightAttack,
		MediumAttack,
		HeavyAttack,
		Guard,
		Pause,
		Submit,
		Cancel,
		Count
	};

	enum class InputDeviceType : uint8_t
	{
		None,
		Keyboard,
		Gamepad
	};

	enum class InputValueType : uint8_t
	{
		Button,
		Axis1D,
		Axis2D
	};

	struct InputValue
	{
		InputValueType type = InputValueType::Button;
		float scalar = 0.0f;
		DirectX::SimpleMath::Vector2 axis = DirectX::SimpleMath::Vector2::Zero;

		static InputValue Button(bool pressed);
		static InputValue Axis1D(float value);
		static InputValue Axis2D(const DirectX::SimpleMath::Vector2& value);

		bool IsActive(float threshold = 0.5f) const;
	};

	struct InputActionState
	{
		InputValue value;
		InputValue previousValue;

		// Trigger: previous frame is not active, current frame is active.
		bool trigger = false;
		// Press: current frame is active.
		bool press = false;
		// Release: previous frame is active, current frame is not active.
		bool release = false;

		InputDeviceType deviceType = InputDeviceType::None;
	};

	using ActionStateArray = std::array<InputActionState, static_cast<size_t>(InputActionId::Count)>;

	struct PlayerInputState
	{
		ActionStateArray actions = {};
		InputDeviceType lastUsedDeviceType = InputDeviceType::None;
		bool keyboardAssigned = false;
		int gamepadIndex = -1;
	};
}
