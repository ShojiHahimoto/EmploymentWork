#include "Data/BattleSetupData.h"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace BattleSetup
{
	namespace
	{
		/// <summary>
		/// 指定プレイヤーの Keyboard Axis2D Binding を追加する。
		/// </summary>
		/// <param name="bindings">追加先 Binding 配列。</param>
		/// <param name="playerIndex">入力を反映する Player 番号。</param>
		/// <param name="config">上下左右キー設定。</param>
		void AddKeyboardMoveBinding(
			std::vector<Input::InputBinding>& bindings,
			int playerIndex,
			const KeyboardKeyConfig& config)
		{
			Input::InputBinding binding;
			binding.actionMap = Input::InputActionMapId::Gameplay;
			binding.action = Input::InputActionId::Move;
			binding.valueType = Input::InputValueType::Axis2D;
			binding.deviceType = Input::BindingDeviceType::Keyboard;
			binding.playerIndex = playerIndex;
			binding.keyboardAxis2D.negativeX = config.moveLeft;
			binding.keyboardAxis2D.positiveX = config.moveRight;
			binding.keyboardAxis2D.negativeY = config.moveDown;
			binding.keyboardAxis2D.positiveY = config.moveUp;
			bindings.push_back(binding);
		}

		/// <summary>
		/// 指定プレイヤーの Keyboard Button Binding を追加する。
		/// </summary>
		/// <param name="bindings">追加先 Binding 配列。</param>
		/// <param name="playerIndex">入力を反映する Player 番号。</param>
		/// <param name="action">反映先 Action。</param>
		/// <param name="key">割り当てる KeyboardKey。</param>
		void AddKeyboardButtonBinding(
			std::vector<Input::InputBinding>& bindings,
			int playerIndex,
			Input::InputActionId action,
			Input::KeyboardKey key)
		{
			Input::InputBinding binding;
			binding.actionMap = Input::InputActionMapId::Gameplay;
			binding.action = action;
			binding.valueType = Input::InputValueType::Button;
			binding.deviceType = Input::BindingDeviceType::Keyboard;
			binding.playerIndex = playerIndex;
			binding.keyboardButton.key = key;
			bindings.push_back(binding);
		}

		/// <summary>
		/// 指定プレイヤーの Gamepad Axis2D Binding を追加する。
		/// </summary>
		/// <param name="bindings">追加先 Binding 配列。</param>
		/// <param name="playerIndex">入力を反映する Player 番号。</param>
		/// <param name="gamepadIndex">XInput ゲームパッド番号。</param>
		/// <param name="source">Axis2D の入力元。</param>
		/// <param name="config">十字方向ボタン設定。</param>
		void AddGamepadMoveBinding(
			std::vector<Input::InputBinding>& bindings,
			int playerIndex,
			int gamepadIndex,
			Input::GamepadAxis2DSource source,
			const GamepadKeyConfig& config)
		{
			Input::InputBinding binding;
			binding.actionMap = Input::InputActionMapId::Gameplay;
			binding.action = Input::InputActionId::Move;
			binding.valueType = Input::InputValueType::Axis2D;
			binding.deviceType = Input::BindingDeviceType::Gamepad;
			binding.playerIndex = playerIndex;
			binding.gamepadAxis2D.gamepadIndex = gamepadIndex;
			binding.gamepadAxis2D.source = source;
			binding.gamepadAxis2D.negativeXButton = config.moveLeft;
			binding.gamepadAxis2D.positiveXButton = config.moveRight;
			binding.gamepadAxis2D.negativeYButton = config.moveDown;
			binding.gamepadAxis2D.positiveYButton = config.moveUp;
			bindings.push_back(binding);
		}

		/// <summary>
		/// 指定プレイヤーの Gamepad Button Binding を追加する。
		/// </summary>
		/// <param name="bindings">追加先 Binding 配列。</param>
		/// <param name="playerIndex">入力を反映する Player 番号。</param>
		/// <param name="gamepadIndex">XInput ゲームパッド番号。</param>
		/// <param name="action">反映先 Action。</param>
		/// <param name="button">割り当てる GamepadButton。</param>
		void AddGamepadButtonBinding(
			std::vector<Input::InputBinding>& bindings,
			int playerIndex,
			int gamepadIndex,
			Input::InputActionId action,
			Input::GamepadButton button)
		{
			Input::InputBinding binding;
			binding.actionMap = Input::InputActionMapId::Gameplay;
			binding.action = action;
			binding.valueType = Input::InputValueType::Button;
			binding.deviceType = Input::BindingDeviceType::Gamepad;
			binding.playerIndex = playerIndex;
			binding.gamepadButton.gamepadIndex = gamepadIndex;
			binding.gamepadButton.button = button;
			bindings.push_back(binding);
		}

		/// <summary>
		/// 既定 Binding から Gameplay 用だけを取り除き、UI 用 Binding を残す。
		/// </summary>
		/// <param name="bindings">整理する Binding 配列。</param>
		void RemoveGameplayBindings(std::vector<Input::InputBinding>& bindings)
		{
			bindings.erase(
				std::remove_if(
					bindings.begin(),
					bindings.end(),
					[](const Input::InputBinding& binding)
					{
						return binding.actionMap == Input::InputActionMapId::Gameplay;
					}),
				bindings.end());
		}

		/// <summary>
		/// プレイヤー設定のキャラクタースロット情報を指定番号に揃える。
		/// </summary>
		/// <param name="player">更新対象の PlayerSetup。</param>
		/// <param name="slotIndex">設定するスロット番号。</param>
		void SetCharacterSlot(PlayerSetup& player, int slotIndex)
		{
			player.characterSlotIndex = std::clamp(slotIndex, 0, CharacterSlotCount - 1);
			player.characterId = BuildCharacterSlotId(player.characterSlotIndex);
			player.characterFolderPath = BuildCharacterFolderPath(player.characterSlotIndex);
		}
	}

	std::string BuildCharacterSlotId(int slotIndex)
	{
		const int clampedSlotIndex = std::clamp(slotIndex, 0, CharacterSlotCount - 1);

		std::ostringstream stream;
		stream << "CharacterSlot" << std::setw(2) << std::setfill('0') << clampedSlotIndex;
		return stream.str();
	}

	std::string BuildCharacterFolderPath(int slotIndex)
	{
		return (std::filesystem::path(CharacterDataRootPath) / BuildCharacterSlotId(slotIndex)).generic_string();
	}

	BattleSetupData CreateDefaultBattleSetupData()
	{
		BattleSetupData setupData;
		setupData.players[0].device.kind = InputDeviceKind::Keyboard;
		setupData.players[0].device.gamepadIndex = -1;
		SetCharacterSlot(setupData.players[0], 0);

		setupData.players[1].device.kind = InputDeviceKind::Gamepad;
		setupData.players[1].device.gamepadIndex = 0;
		SetCharacterSlot(setupData.players[1], 0);

		return setupData;
	}

	bool AreDevicesUnique(const BattleSetupData& setupData)
	{
		const InputDeviceAssignment& player1 = setupData.players[0].device;
		const InputDeviceAssignment& player2 = setupData.players[1].device;
		if (player1.kind != player2.kind)
		{
			return true;
		}

		if (player1.kind == InputDeviceKind::Keyboard)
		{
			return false;
		}

		return player1.gamepadIndex != player2.gamepadIndex;
	}

	std::vector<Input::InputBinding> BuildInputBindings(const BattleSetupData& setupData)
	{
		std::vector<Input::InputBinding> bindings = Input::CreateDefaultInputBindings();
		RemoveGameplayBindings(bindings);

		for (int playerIndex = 0; playerIndex < Input::MaxPlayers; ++playerIndex)
		{
			const PlayerSetup& player = setupData.players[static_cast<size_t>(playerIndex)];
			if (player.device.kind == InputDeviceKind::Keyboard)
			{
				const KeyboardKeyConfig& config = player.keyboardConfig;
				AddKeyboardMoveBinding(bindings, playerIndex, config);
				AddKeyboardButtonBinding(bindings, playerIndex, Input::InputActionId::AttackA, config.attackA);
				AddKeyboardButtonBinding(bindings, playerIndex, Input::InputActionId::AttackB, config.attackB);
				AddKeyboardButtonBinding(bindings, playerIndex, Input::InputActionId::AttackX, config.attackX);
				AddKeyboardButtonBinding(bindings, playerIndex, Input::InputActionId::AttackY, config.attackY);
				AddKeyboardButtonBinding(bindings, playerIndex, Input::InputActionId::Pause, Input::KeyboardKey::Escape);
				continue;
			}

			const int gamepadIndex = player.device.gamepadIndex;
			const GamepadKeyConfig& config = player.gamepadConfig;
			AddGamepadMoveBinding(bindings, playerIndex, gamepadIndex, Input::GamepadAxis2DSource::LeftStick, config);
			AddGamepadMoveBinding(bindings, playerIndex, gamepadIndex, Input::GamepadAxis2DSource::DPad, config);
			AddGamepadButtonBinding(bindings, playerIndex, gamepadIndex, Input::InputActionId::AttackA, config.attackA);
			AddGamepadButtonBinding(bindings, playerIndex, gamepadIndex, Input::InputActionId::AttackB, config.attackB);
			AddGamepadButtonBinding(bindings, playerIndex, gamepadIndex, Input::InputActionId::AttackX, config.attackX);
			AddGamepadButtonBinding(bindings, playerIndex, gamepadIndex, Input::InputActionId::AttackY, config.attackY);
			AddGamepadButtonBinding(bindings, playerIndex, gamepadIndex, Input::InputActionId::Pause, Input::GamepadButton::Start);
		}

		return bindings;
	}
}
