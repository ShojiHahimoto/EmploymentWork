#pragma once

#include "Input/InputBindings.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace BattleSetup
{
	constexpr int CharacterSlotCount = 10;
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

	/// <summary>
	/// キャラクタースロット番号から保存フォルダ名に使う ID を作る。
	/// </summary>
	/// <param name="slotIndex">キャラクタースロット番号。</param>
	/// <returns>CharacterSlot00 のような固定桁の ID。</returns>
	std::string BuildCharacterSlotId(int slotIndex);

	/// <summary>
	/// キャラクタースロット番号から CharacterData 保存フォルダパスを作る。
	/// </summary>
	/// <param name="slotIndex">キャラクタースロット番号。</param>
	/// <returns>assets/CharacterData 配下の対象フォルダパス。</returns>
	std::string BuildCharacterFolderPath(int slotIndex);

	/// <summary>
	/// 開発用直行にも使う既定のバトル設定を作る。
	/// </summary>
	/// <returns>1P Keyboard、2P XInput Pad0、両者 CharacterSlot00 の設定。</returns>
	BattleSetupData CreateDefaultBattleSetupData();

	/// <summary>
	/// 1P/2P に同じ物理デバイスが割り当てられていないか確認する。
	/// </summary>
	/// <param name="setupData">確認するバトル開始設定。</param>
	/// <returns>同一デバイスが重複していなければ true。</returns>
	bool AreDevicesUnique(const BattleSetupData& setupData);

	/// <summary>
	/// BattleSetupData から Gameplay 用 Binding を構築し、既定 UI Binding と合わせて返す。
	/// </summary>
	/// <param name="setupData">BattleScene に反映する入力設定。</param>
	/// <returns>InputSystem に渡す Binding 配列。</returns>
	std::vector<Input::InputBinding> BuildInputBindings(const BattleSetupData& setupData);
}
