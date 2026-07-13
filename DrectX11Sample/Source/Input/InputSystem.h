#pragma once

#include "Input/InputBindings.h"

#include <array>
#include <vector>

namespace Input
{
	class InputSystem
	{
	public:
		static void Initialize();
		static void Update();
		static void Shutdown();

		static void SetActionMap(InputActionMapId actionMap);
		static InputActionMapId GetActionMap();

		static const InputActionState& GetActionState(int playerIndex, InputActionId action);
		static const PlayerInputState& GetPlayerInputState(int playerIndex);
		static InputDeviceType GetLastUsedDeviceType(int playerIndex);

		static void SetBindings(const std::vector<InputBinding>& newBindings);
		static const std::vector<InputBinding>& GetBindings();

		static void SetSettings(const InputSettings& newSettings);
		static const InputSettings& GetSettings();

	private:
		static InputActionMapId currentActionMap;
		static InputSettings settings;
		static std::vector<InputBinding> bindings;
		static std::array<PlayerInputState, MaxPlayers> players;
		static InputActionState emptyActionState;

		static void UpdatePreviousValues();
		static void ClearCurrentValues();
		static void ApplyBinding(const InputBinding& binding);
		static void FinalizeActionStates();

		static bool IsValidPlayerIndex(int playerIndex);
		static bool IsKeyboardKeyDown(KeyboardKey key);
		static void MergeActionValue(PlayerInputState& player, const InputBinding& binding, const InputValue& value, InputDeviceType deviceType);
	};
}
