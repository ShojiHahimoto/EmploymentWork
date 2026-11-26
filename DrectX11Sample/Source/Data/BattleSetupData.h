#pragma once

#include "Input/InputBindings.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace BattleSetup
{
	constexpr int CharacterSlotCount = 12;
	constexpr const char* CharacterDataRootPath = "assets/CharacterData";

	enum class InputDeviceKind : uint8_t
	{
		Keyboard,
		Gamepad
	};

	struct InputDeviceAssignment
	{
		// Keyboard は gamepadIndex を使わない。Gamepad の場合だけ XInput 番号を参照する。
		InputDeviceKind kind = InputDeviceKind::Keyboard;
		int gamepadIndex = -1;
	};

	struct KeyboardKeyConfig
	{
		// キーボードは上下左右を独立キーとして保持し、Move Axis2D に変換する。
		Input::KeyboardKey moveLeft = Input::KeyboardKey::A;
		Input::KeyboardKey moveRight = Input::KeyboardKey::D;
		Input::KeyboardKey moveDown = Input::KeyboardKey::S;
		Input::KeyboardKey moveUp = Input::KeyboardKey::W;
		Input::KeyboardKey attackA = Input::KeyboardKey::H;
		Input::KeyboardKey attackB = Input::KeyboardKey::J;
		Input::KeyboardKey attackX = Input::KeyboardKey::Y;
		Input::KeyboardKey attackY = Input::KeyboardKey::U;
	};

	struct GamepadKeyConfig
	{
		// 十字方向もボタン扱いで保持する。左スティック入力は別途固定で併用する。
		Input::GamepadButton moveLeft = Input::GamepadButton::DPadLeft;
		Input::GamepadButton moveRight = Input::GamepadButton::DPadRight;
		Input::GamepadButton moveDown = Input::GamepadButton::DPadDown;
		Input::GamepadButton moveUp = Input::GamepadButton::DPadUp;
		Input::GamepadButton attackA = Input::GamepadButton::A;
		Input::GamepadButton attackB = Input::GamepadButton::B;
		Input::GamepadButton attackX = Input::GamepadButton::X;
		Input::GamepadButton attackY = Input::GamepadButton::Y;
	};

	struct PlayerSetup
	{
		InputDeviceAssignment device;
		bool cpuControlled = false; // この側には対戦用の物理入力を割り当てない。
		int characterSlotIndex = 0;
		std::string characterId = "CharacterSlot00";
		std::string characterFolderPath = "assets/CharacterData/CharacterSlot00";
		std::string keyboardConfigId = "default_keyboard";
		std::string keyboardConfigName = "Default Keyboard";
		std::string gamepadConfigId = "default_gamepad";
		std::string gamepadConfigName = "Default Gamepad";
		KeyboardKeyConfig keyboardConfig;
		GamepadKeyConfig gamepadConfig;
	};

	struct BattleSetupData
	{
		std::array<PlayerSetup, Input::MaxPlayers> players = {};
	};

	std::string BuildCharacterSlotId(int slotIndex);

	std::string BuildCharacterFolderPath(int slotIndex);

	BattleSetupData CreateDefaultBattleSetupData();

	bool AreDevicesUnique(const BattleSetupData& setupData);

	std::vector<Input::InputBinding> BuildInputBindings(const BattleSetupData& setupData);
}
