#include "Input/InputSystem.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>

using namespace DirectX::SimpleMath;

namespace Input
{
	InputActionMapId InputSystem::currentActionMap = InputActionMapId::Gameplay;
	InputSettings InputSystem::settings = {};
	std::vector<InputBinding> InputSystem::bindings;
	std::array<PlayerInputState, MaxPlayers> InputSystem::players = {};
	InputActionState InputSystem::emptyActionState = {};

	InputValue InputValue::Button(bool pressed)
	{
		InputValue value;
		value.type = InputValueType::Button;
		value.scalar = pressed ? 1.0f : 0.0f;
		value.axis = Vector2(value.scalar, 0.0f);
		return value;
	}

	InputValue InputValue::Axis1D(float value)
	{
		InputValue inputValue;
		inputValue.type = InputValueType::Axis1D;
		inputValue.scalar = value;
		inputValue.axis = Vector2(value, 0.0f);
		return inputValue;
	}

	InputValue InputValue::Axis2D(const Vector2& value)
	{
		InputValue inputValue;
		inputValue.type = InputValueType::Axis2D;
		inputValue.scalar = value.Length();
		inputValue.axis = value;
		return inputValue;
	}

	bool InputValue::IsActive(float threshold) const
	{
		switch (type)
		{
		case InputValueType::Axis2D:
			return axis.LengthSquared() >= threshold * threshold;
		case InputValueType::Axis1D:
		case InputValueType::Button:
		default:
			return std::abs(scalar) >= threshold;
		}
	}

	std::vector<InputBinding> CreateDefaultInputBindings()
	{
		std::vector<InputBinding> defaultBindings;

		auto addKeyboardButton = [&defaultBindings](
			InputActionMapId actionMap,
			InputActionId action,
			KeyboardKey key)
			{
				InputBinding binding;
				binding.actionMap = actionMap;
				binding.action = action;
				binding.valueType = InputValueType::Button;
				binding.deviceType = BindingDeviceType::Keyboard;
				binding.playerIndex = 0;
				binding.keyboardButton.key = key;
				defaultBindings.push_back(binding);
			};

		auto addKeyboardAxis2D = [&defaultBindings](
			InputActionMapId actionMap,
			InputActionId action,
			KeyboardKey negativeX,
			KeyboardKey positiveX,
			KeyboardKey negativeY,
			KeyboardKey positiveY)
			{
				InputBinding binding;
				binding.actionMap = actionMap;
				binding.action = action;
				binding.valueType = InputValueType::Axis2D;
				binding.deviceType = BindingDeviceType::Keyboard;
				binding.playerIndex = 0;
				binding.keyboardAxis2D.negativeX = negativeX;
				binding.keyboardAxis2D.positiveX = positiveX;
				binding.keyboardAxis2D.negativeY = negativeY;
				binding.keyboardAxis2D.positiveY = positiveY;
				defaultBindings.push_back(binding);
			};

		addKeyboardAxis2D(InputActionMapId::Gameplay, InputActionId::Move, KeyboardKey::A, KeyboardKey::D, KeyboardKey::S, KeyboardKey::W);
		addKeyboardButton(InputActionMapId::Gameplay, InputActionId::Jump, KeyboardKey::Space);
		addKeyboardButton(InputActionMapId::Gameplay, InputActionId::LightAttack, KeyboardKey::J);
		addKeyboardButton(InputActionMapId::Gameplay, InputActionId::MediumAttack, KeyboardKey::K);
		addKeyboardButton(InputActionMapId::Gameplay, InputActionId::HeavyAttack, KeyboardKey::L);
		addKeyboardButton(InputActionMapId::Gameplay, InputActionId::Guard, KeyboardKey::I);
		addKeyboardButton(InputActionMapId::Gameplay, InputActionId::Pause, KeyboardKey::Escape);

		addKeyboardAxis2D(InputActionMapId::UI, InputActionId::Move, KeyboardKey::Left, KeyboardKey::Right, KeyboardKey::Down, KeyboardKey::Up);
		addKeyboardButton(InputActionMapId::UI, InputActionId::Submit, KeyboardKey::Enter);
		addKeyboardButton(InputActionMapId::UI, InputActionId::Submit, KeyboardKey::Space);
		addKeyboardButton(InputActionMapId::UI, InputActionId::Cancel, KeyboardKey::Escape);

		return defaultBindings;
	}

	void InputSystem::Initialize()
	{
		settings = InputSettings{};
		bindings = CreateDefaultInputBindings();
		currentActionMap = InputActionMapId::Gameplay;

		for (PlayerInputState& player : players)
		{
			player = PlayerInputState{};
		}

		players[0].keyboardAssigned = true;
	}

	void InputSystem::Update()
	{
		UpdatePreviousValues();
		ClearCurrentValues();

		for (const InputBinding& binding : bindings)
		{
			if (binding.actionMap == currentActionMap)
			{
				ApplyBinding(binding);
			}
		}

		FinalizeActionStates();
	}

	void InputSystem::Shutdown()
	{
		bindings.clear();
		for (PlayerInputState& player : players)
		{
			player = PlayerInputState{};
		}
		currentActionMap = InputActionMapId::Gameplay;
	}

	void InputSystem::SetActionMap(InputActionMapId actionMap)
	{
		if (currentActionMap == actionMap)
		{
			return;
		}

		currentActionMap = actionMap;
		for (PlayerInputState& player : players)
		{
			for (InputActionState& action : player.actions)
			{
				action = InputActionState{};
			}
		}
	}

	InputActionMapId InputSystem::GetActionMap()
	{
		return currentActionMap;
	}

	const InputActionState& InputSystem::GetActionState(int playerIndex, InputActionId action)
	{
		if (!IsValidPlayerIndex(playerIndex))
		{
			return emptyActionState;
		}

		const size_t actionIndex = static_cast<size_t>(action);
		if (actionIndex >= players[playerIndex].actions.size())
		{
			return emptyActionState;
		}

		return players[playerIndex].actions[actionIndex];
	}

	const PlayerInputState& InputSystem::GetPlayerInputState(int playerIndex)
	{
		if (!IsValidPlayerIndex(playerIndex))
		{
			return players[0];
		}

		return players[playerIndex];
	}

	InputDeviceType InputSystem::GetLastUsedDeviceType(int playerIndex)
	{
		if (!IsValidPlayerIndex(playerIndex))
		{
			return InputDeviceType::None;
		}

		return players[playerIndex].lastUsedDeviceType;
	}

	void InputSystem::SetBindings(const std::vector<InputBinding>& newBindings)
	{
		bindings = newBindings;
	}

	const std::vector<InputBinding>& InputSystem::GetBindings()
	{
		return bindings;
	}

	void InputSystem::SetSettings(const InputSettings& newSettings)
	{
		settings = newSettings;
	}

	const InputSettings& InputSystem::GetSettings()
	{
		return settings;
	}

	void InputSystem::UpdatePreviousValues()
	{
		for (PlayerInputState& player : players)
		{
			for (InputActionState& action : player.actions)
			{
				action.previousValue = action.value;
				action.trigger = false;
				action.press = false;
				action.release = false;
				action.deviceType = InputDeviceType::None;
			}
		}
	}

	void InputSystem::ClearCurrentValues()
	{
		for (PlayerInputState& player : players)
		{
			for (InputActionState& action : player.actions)
			{
				action.value = InputValue{};
			}
		}
	}

	void InputSystem::ApplyBinding(const InputBinding& binding)
	{
		if (!IsValidPlayerIndex(binding.playerIndex))
		{
			return;
		}

		PlayerInputState& player = players[binding.playerIndex];
		if (binding.deviceType == BindingDeviceType::Keyboard && !player.keyboardAssigned)
		{
			return;
		}

		if (binding.deviceType != BindingDeviceType::Keyboard)
		{
			return;
		}

		InputValue value;
		if (binding.valueType == InputValueType::Button)
		{
			value = InputValue::Button(IsKeyboardKeyDown(binding.keyboardButton.key));
		}
		else if (binding.valueType == InputValueType::Axis2D)
		{
			Vector2 axis = Vector2::Zero;
			if (IsKeyboardKeyDown(binding.keyboardAxis2D.negativeX)) axis.x -= 1.0f;
			if (IsKeyboardKeyDown(binding.keyboardAxis2D.positiveX)) axis.x += 1.0f;
			if (IsKeyboardKeyDown(binding.keyboardAxis2D.negativeY)) axis.y -= 1.0f;
			if (IsKeyboardKeyDown(binding.keyboardAxis2D.positiveY)) axis.y += 1.0f;

			if (axis.LengthSquared() > 1.0f)
			{
				axis.Normalize();
			}

			value = InputValue::Axis2D(axis);
		}

		MergeActionValue(player, binding, value, InputDeviceType::Keyboard);
	}

	void InputSystem::FinalizeActionStates()
	{
		for (PlayerInputState& player : players)
		{
			for (InputActionState& action : player.actions)
			{
				const bool previousActive = action.previousValue.IsActive(settings.buttonThreshold);
				const bool currentActive = action.value.IsActive(settings.buttonThreshold);

				action.trigger = !previousActive && currentActive;
				action.press = currentActive;
				action.release = previousActive && !currentActive;

				if (currentActive && action.deviceType != InputDeviceType::None)
				{
					player.lastUsedDeviceType = action.deviceType;
				}
			}
		}
	}

	bool InputSystem::IsValidPlayerIndex(int playerIndex)
	{
		return playerIndex >= 0 && playerIndex < MaxPlayers;
	}

	bool InputSystem::IsKeyboardKeyDown(KeyboardKey key)
	{
		if (key == KeyboardKey::None)
		{
			return false;
		}

		return (GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;
	}

	void InputSystem::MergeActionValue(PlayerInputState& player, const InputBinding& binding, const InputValue& value, InputDeviceType deviceType)
	{
		const size_t actionIndex = static_cast<size_t>(binding.action);
		if (actionIndex >= player.actions.size())
		{
			return;
		}

		InputActionState& action = player.actions[actionIndex];
		action.value.type = binding.valueType;

		if (binding.valueType == InputValueType::Axis2D)
		{
			action.value.axis += value.axis;
			if (action.value.axis.LengthSquared() > 1.0f)
			{
				action.value.axis.Normalize();
			}
			action.value.scalar = action.value.axis.Length();
		}
		else
		{
			action.value.scalar = std::max(action.value.scalar, value.scalar);
			action.value.axis = Vector2(action.value.scalar, 0.0f);
		}

		if (value.IsActive(settings.buttonThreshold))
		{
			action.deviceType = deviceType;
		}
	}
}
