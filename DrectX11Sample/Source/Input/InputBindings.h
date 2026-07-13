#pragma once

#include "Input/InputTypes.h"

#include <vector>

namespace Input
{
	enum class KeyboardKey : int
	{
		None = 0,

		A = 'A',
		D = 'D',
		I = 'I',
		J = 'J',
		K = 'K',
		L = 'L',
		Q = 'Q',
		S = 'S',
		W = 'W',

		Down = 0x28,
		Enter = 0x0D,
		Escape = 0x1B,
		Left = 0x25,
		Right = 0x27,
		Space = 0x20,
		Up = 0x26
	};

	enum class BindingDeviceType : uint8_t
	{
		Keyboard,
		Gamepad
	};

	struct KeyboardButtonBinding
	{
		KeyboardKey key = KeyboardKey::None;
	};

	struct KeyboardAxis2DBinding
	{
		KeyboardKey negativeX = KeyboardKey::None;
		KeyboardKey positiveX = KeyboardKey::None;
		KeyboardKey negativeY = KeyboardKey::None;
		KeyboardKey positiveY = KeyboardKey::None;
	};

	struct InputBinding
	{
		InputActionMapId actionMap = InputActionMapId::Gameplay;
		InputActionId action = InputActionId::Move;
		InputValueType valueType = InputValueType::Button;
		BindingDeviceType deviceType = BindingDeviceType::Keyboard;
		int playerIndex = 0;

		KeyboardButtonBinding keyboardButton;
		KeyboardAxis2DBinding keyboardAxis2D;
	};

	struct InputSettings
	{
		float buttonThreshold = 0.5f;
		float axisDeadZone = 0.25f;
	};

	std::vector<InputBinding> CreateDefaultInputBindings();
}
