#include "Input/InputSystem.h"

#include <Windows.h>
#include <Xinput.h>

#include <algorithm>
#include <array>
#include <cmath>

#pragma comment(lib, "xinput.lib")

using namespace DirectX::SimpleMath;

namespace Input
{
	namespace
	{
		constexpr int MaxXInputGamepads = 4;

		std::array<XINPUT_STATE, MaxXInputGamepads> cachedGamepadStates = {};
		std::array<bool, MaxXInputGamepads> cachedGamepadConnected = {};

		/// <summary>
		/// プロジェクト内の GamepadButton を XInput のボタンビットへ変換する。
		/// </summary>
		/// <param name="button">変換するゲームパッドボタン。</param>
		/// <returns>XInput の wButtons と比較するビット。None は 0。</returns>
		WORD ToXInputButton(GamepadButton button)
		{
			switch (button)
			{
			case GamepadButton::DPadUp: return XINPUT_GAMEPAD_DPAD_UP;
			case GamepadButton::DPadDown: return XINPUT_GAMEPAD_DPAD_DOWN;
			case GamepadButton::DPadLeft: return XINPUT_GAMEPAD_DPAD_LEFT;
			case GamepadButton::DPadRight: return XINPUT_GAMEPAD_DPAD_RIGHT;
			case GamepadButton::Start: return XINPUT_GAMEPAD_START;
			case GamepadButton::Back: return XINPUT_GAMEPAD_BACK;
			case GamepadButton::LeftThumb: return XINPUT_GAMEPAD_LEFT_THUMB;
			case GamepadButton::RightThumb: return XINPUT_GAMEPAD_RIGHT_THUMB;
			case GamepadButton::LeftShoulder: return XINPUT_GAMEPAD_LEFT_SHOULDER;
			case GamepadButton::RightShoulder: return XINPUT_GAMEPAD_RIGHT_SHOULDER;
			case GamepadButton::LeftTrigger:
			case GamepadButton::RightTrigger:
				return 0;
			case GamepadButton::A: return XINPUT_GAMEPAD_A;
			case GamepadButton::B: return XINPUT_GAMEPAD_B;
			case GamepadButton::X: return XINPUT_GAMEPAD_X;
			case GamepadButton::Y: return XINPUT_GAMEPAD_Y;
			case GamepadButton::None:
			default:
				return 0;
			}
		}

		/// <summary>
		/// XInput の SHORT スティック値を -1.0〜1.0 に正規化する。
		/// </summary>
		/// <param name="value">XInput から取得したスティック値。</param>
		/// <returns>正規化済みの軸入力。</returns>
		float NormalizeThumbValue(SHORT value)
		{
			if (value < 0)
			{
				return static_cast<float>(value) / 32768.0f;
			}

			return static_cast<float>(value) / 32767.0f;
		}

		/// <summary>
		/// スティックの中央付近を無入力にし、外側の値を滑らかに 0〜1 へ再配置する。
		/// </summary>
		/// <param name="axis">正規化済みの生スティック入力。</param>
		/// <param name="deadZone">無入力扱いにする中心半径。</param>
		/// <returns>デッドゾーン適用後のスティック入力。</returns>
		Vector2 ApplyRadialDeadZone(const Vector2& axis, float deadZone)
		{
			const float length = axis.Length();
			if (length <= deadZone)
			{
				return Vector2::Zero;
			}

			const float safeRange = std::max(1.0f - deadZone, 0.001f);
			const float normalizedLength = std::clamp((length - deadZone) / safeRange, 0.0f, 1.0f);
			const Vector2 direction = axis / length;
			return direction * normalizedLength;
		}

	}

	// InputSystem は static なフレーム入力状態を持つ。
	// Scene / System はここから同一フレーム内で同じ結果を読む。
	InputActionMapId InputSystem::currentActionMap = InputActionMapId::Gameplay;
	InputSettings InputSystem::settings = {};
	std::vector<InputBinding> InputSystem::bindings;
	std::array<PlayerInputState, MaxPlayers> InputSystem::players = {};
	InputActionState InputSystem::emptyActionState = {};

	/// <summary>
	/// Button 入力を InputValue の 0/1 値に変換する。
	/// </summary>
	/// <param name="pressed">ボタンが押されているかどうか。</param>
	/// <returns>Button 型の InputValue。</returns>
	InputValue InputValue::Button(bool pressed)
	{
		InputValue value;
		value.type = InputValueType::Button;
		value.scalar = pressed ? 1.0f : 0.0f;
		value.axis = Vector2(value.scalar, 0.0f);
		return value;
	}

	/// <summary>
	/// 1 軸入力を InputValue に変換する。
	/// </summary>
	/// <param name="value">1 軸の入力値。</param>
	/// <returns>Axis1D 型の InputValue。</returns>
	InputValue InputValue::Axis1D(float value)
	{
		InputValue inputValue;
		inputValue.type = InputValueType::Axis1D;
		inputValue.scalar = value;
		inputValue.axis = Vector2(value, 0.0f);
		return inputValue;
	}

	/// <summary>
	/// 2 軸入力を InputValue に変換する。
	/// </summary>
	/// <param name="value">2 軸の入力値。</param>
	/// <returns>Axis2D 型の InputValue。</returns>
	InputValue InputValue::Axis2D(const Vector2& value)
	{
		InputValue inputValue;
		inputValue.type = InputValueType::Axis2D;
		inputValue.scalar = value.Length();
		inputValue.axis = value;
		return inputValue;
	}

	/// <summary>
	/// 入力値がしきい値を超えているか判定する。
	/// </summary>
	/// <param name="threshold">入力中とみなすしきい値。</param>
	/// <returns>入力が有効なら true。</returns>
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

	/// <summary>
	/// 初期状態で使うキーボード / ゲームパッド Binding を作る。
	/// </summary>
	/// <returns>Gameplay と UI の既定 Binding 配列。</returns>
	std::vector<InputBinding> CreateDefaultInputBindings()
	{
		std::vector<InputBinding> defaultBindings;

		// Button Action 用のキーボード Binding を 1 件追加する小さな補助。
		auto addKeyboardButton = [&defaultBindings](
			InputActionMapId actionMap,
			InputActionId action,
			KeyboardKey key,
			int playerIndex = 0)
			{
				InputBinding binding;
				binding.actionMap = actionMap;
				binding.action = action;
				binding.valueType = InputValueType::Button;
				binding.deviceType = BindingDeviceType::Keyboard;
				binding.playerIndex = playerIndex;
				binding.keyboardButton.key = key;
				defaultBindings.push_back(binding);
			};

		// WASD や矢印キーなど、4 キーから Axis2D Action を作るキーボード Binding を追加する。
		auto addKeyboardAxis2D = [&defaultBindings](
			InputActionMapId actionMap,
			InputActionId action,
			KeyboardKey negativeX,
			KeyboardKey positiveX,
			KeyboardKey negativeY,
			KeyboardKey positiveY,
			int playerIndex = 0)
			{
				InputBinding binding;
				binding.actionMap = actionMap;
				binding.action = action;
				binding.valueType = InputValueType::Axis2D;
				binding.deviceType = BindingDeviceType::Keyboard;
				binding.playerIndex = playerIndex;
				binding.keyboardAxis2D.negativeX = negativeX;
				binding.keyboardAxis2D.positiveX = positiveX;
				binding.keyboardAxis2D.negativeY = negativeY;
				binding.keyboardAxis2D.positiveY = positiveY;
				defaultBindings.push_back(binding);
			};

		// Button Action 用のゲームパッド Binding を 1 件追加する小さな補助。
		auto addGamepadButton = [&defaultBindings](
			InputActionMapId actionMap,
			InputActionId action,
			int playerIndex,
			int gamepadIndex,
			GamepadButton button)
			{
				InputBinding binding;
				binding.actionMap = actionMap;
				binding.action = action;
				binding.valueType = InputValueType::Button;
				binding.deviceType = BindingDeviceType::Gamepad;
				binding.playerIndex = playerIndex;
				binding.gamepadButton.gamepadIndex = gamepadIndex;
				binding.gamepadButton.button = button;
				defaultBindings.push_back(binding);
			};

		// スティックや十字キーから Axis2D Action を作るゲームパッド Binding を追加する。
		auto addGamepadAxis2D = [&defaultBindings](
			InputActionMapId actionMap,
			InputActionId action,
			int playerIndex,
			int gamepadIndex,
			GamepadAxis2DSource source)
			{
				InputBinding binding;
				binding.actionMap = actionMap;
				binding.action = action;
				binding.valueType = InputValueType::Axis2D;
				binding.deviceType = BindingDeviceType::Gamepad;
				binding.playerIndex = playerIndex;
				binding.gamepadAxis2D.gamepadIndex = gamepadIndex;
				binding.gamepadAxis2D.source = source;
				defaultBindings.push_back(binding);
			};

		// Gameplay はキャラクター操作用。現状は Player 0 のキーボードだけに割り当てる。
		addKeyboardAxis2D(InputActionMapId::Gameplay, InputActionId::Move, KeyboardKey::A, KeyboardKey::D, KeyboardKey::S, KeyboardKey::W);
		addKeyboardButton(InputActionMapId::Gameplay, InputActionId::AttackA, KeyboardKey::H);
		addKeyboardButton(InputActionMapId::Gameplay, InputActionId::AttackB, KeyboardKey::J);
		addKeyboardButton(InputActionMapId::Gameplay, InputActionId::AttackX, KeyboardKey::Y);
		addKeyboardButton(InputActionMapId::Gameplay, InputActionId::AttackY, KeyboardKey::U);
		addKeyboardButton(InputActionMapId::Gameplay, InputActionId::Guard, KeyboardKey::I);
		addKeyboardButton(InputActionMapId::Gameplay, InputActionId::Pause, KeyboardKey::Escape);

		// 仮の 2P 操作。ゲームパッド 0 が接続されていれば Player 1 に入力が入る。
		// 将来は JSON の Binding 差し替えで 1P/2P とデバイス番号を自由に変更する。
		constexpr int Player2Index = 1;
		constexpr int DefaultGamepadIndex = 0;
		addGamepadAxis2D(InputActionMapId::Gameplay, InputActionId::Move, Player2Index, DefaultGamepadIndex, GamepadAxis2DSource::LeftStick);
		addGamepadAxis2D(InputActionMapId::Gameplay, InputActionId::Move, Player2Index, DefaultGamepadIndex, GamepadAxis2DSource::DPad);
		addGamepadButton(InputActionMapId::Gameplay, InputActionId::AttackA, Player2Index, DefaultGamepadIndex, GamepadButton::A);
		addGamepadButton(InputActionMapId::Gameplay, InputActionId::AttackB, Player2Index, DefaultGamepadIndex, GamepadButton::B);
		addGamepadButton(InputActionMapId::Gameplay, InputActionId::AttackX, Player2Index, DefaultGamepadIndex, GamepadButton::X);
		addGamepadButton(InputActionMapId::Gameplay, InputActionId::AttackY, Player2Index, DefaultGamepadIndex, GamepadButton::Y);
		addGamepadButton(InputActionMapId::Gameplay, InputActionId::Guard, Player2Index, DefaultGamepadIndex, GamepadButton::LeftShoulder);
		addGamepadButton(InputActionMapId::Gameplay, InputActionId::Pause, Player2Index, DefaultGamepadIndex, GamepadButton::Start);

		// UI はメニュー操作用。Gameplay と同時には有効にしない。
		addKeyboardAxis2D(InputActionMapId::UI, InputActionId::Move, KeyboardKey::Left, KeyboardKey::Right, KeyboardKey::Down, KeyboardKey::Up);
		addKeyboardButton(InputActionMapId::UI, InputActionId::Submit, KeyboardKey::F);
		addKeyboardButton(InputActionMapId::UI, InputActionId::Cancel, KeyboardKey::Escape);
		for (int gamepadIndex = 0; gamepadIndex < MaxXInputGamepads; ++gamepadIndex)
		{
			addGamepadAxis2D(InputActionMapId::UI, InputActionId::Move, 0, gamepadIndex, GamepadAxis2DSource::LeftStick);
			addGamepadAxis2D(InputActionMapId::UI, InputActionId::Move, 0, gamepadIndex, GamepadAxis2DSource::DPad);
			addGamepadButton(InputActionMapId::UI, InputActionId::Submit, 0, gamepadIndex, GamepadButton::A);
			addGamepadButton(InputActionMapId::UI, InputActionId::Cancel, 0, gamepadIndex, GamepadButton::B);
		}

		return defaultBindings;
	}

	/// <summary>
	/// 入力設定、既定 Binding、Player 割り当てを初期化する。
	/// </summary>
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
		players[1].gamepadIndex = 0;
	}

	/// <summary>
	/// 1 フレーム分の入力を取得し、ActionState の Trigger / Press / Release を確定する。
	/// </summary>
	void InputSystem::Update()
	{
		UpdatePreviousValues();
		ClearCurrentValues();
		UpdateGamepadStates();

		for (const InputBinding& binding : bindings)
		{
			if (binding.actionMap == currentActionMap)
			{
				ApplyBinding(binding);
			}
		}

		FinalizeActionStates();
	}

	/// <summary>
	/// InputSystem が保持する Binding と Player 入力状態を破棄する。
	/// </summary>
	void InputSystem::Shutdown()
	{
		bindings.clear();
		for (PlayerInputState& player : players)
		{
			player = PlayerInputState{};
		}
		currentActionMap = InputActionMapId::Gameplay;
	}

	/// <summary>
	/// ゲーム全体で有効な ActionMap を切り替える。
	/// </summary>
	/// <param name="actionMap">切り替え先の ActionMap。</param>
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

	/// <summary>
	/// 現在有効な ActionMap を取得する。
	/// </summary>
	/// <returns>現在の ActionMap。</returns>
	InputActionMapId InputSystem::GetActionMap()
	{
		return currentActionMap;
	}

	/// <summary>
	/// 指定 Player の指定 Action 状態を取得する。
	/// </summary>
	/// <param name="playerIndex">取得する Player 番号。</param>
	/// <param name="action">取得する Action ID。</param>
	/// <returns>ActionState。範囲外の場合は空の状態。</returns>
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

	/// <summary>
	/// 指定 Player の全 Action 入力状態を取得する。
	/// </summary>
	/// <param name="playerIndex">取得する Player 番号。</param>
	/// <returns>PlayerInputState。範囲外の場合は Player 0 の状態。</returns>
	const PlayerInputState& InputSystem::GetPlayerInputState(int playerIndex)
	{
		if (!IsValidPlayerIndex(playerIndex))
		{
			return players[0];
		}

		return players[playerIndex];
	}

	/// <summary>
	/// 指定 Player が最後に使用した入力デバイス種別を取得する。
	/// </summary>
	/// <param name="playerIndex">取得する Player 番号。</param>
	/// <returns>最後に有効入力を出したデバイス種別。</returns>
	InputDeviceType InputSystem::GetLastUsedDeviceType(int playerIndex)
	{
		if (!IsValidPlayerIndex(playerIndex))
		{
			return InputDeviceType::None;
		}

		return players[playerIndex].lastUsedDeviceType;
	}

	/// <summary>
	/// InputSystem が使う Binding 一式を差し替える。
	/// </summary>
	/// <param name="newBindings">新しく使う Binding 配列。</param>
	void InputSystem::SetBindings(const std::vector<InputBinding>& newBindings)
	{
		bindings = newBindings;
		for (PlayerInputState& player : players)
		{
			for (InputActionState& action : player.actions)
			{
				action = InputActionState{};
			}
		}
	}

	/// <summary>
	/// 現在設定されている Binding 一覧を取得する。
	/// </summary>
	/// <returns>読み取り専用の Binding 配列。</returns>
	const std::vector<InputBinding>& InputSystem::GetBindings()
	{
		return bindings;
	}

	/// <summary>
	/// 入力しきい値などの InputSettings を差し替える。
	/// </summary>
	/// <param name="newSettings">新しく使う入力設定。</param>
	void InputSystem::SetSettings(const InputSettings& newSettings)
	{
		settings = newSettings;
	}

	/// <summary>
	/// 現在の InputSettings を取得する。
	/// </summary>
	/// <returns>読み取り専用の入力設定。</returns>
	const InputSettings& InputSystem::GetSettings()
	{
		return settings;
	}

	/// <summary>
	/// InputSystem が確認する XInput ゲームパッドの最大数を取得する。
	/// </summary>
	/// <returns>XInput のゲームパッド最大数。</returns>
	int InputSystem::GetMaxGamepadCount()
	{
		return MaxXInputGamepads;
	}

	/// <summary>
	/// 指定ゲームパッドが現在接続されているか確認する。
	/// </summary>
	/// <param name="gamepadIndex">確認する XInput ゲームパッド番号。</param>
	/// <returns>接続されていれば true。</returns>
	bool InputSystem::IsGamepadConnected(int gamepadIndex)
	{
		return IsValidGamepadIndex(gamepadIndex) && cachedGamepadConnected[gamepadIndex];
	}

	/// <summary>
	/// 今フレーム値を前フレーム値へ退避し、Trigger / Press / Release を初期化する。
	/// </summary>
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

	/// <summary>
	/// Binding 適用前に、今フレームの入力値だけを初期化する。
	/// </summary>
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

	/// <summary>
	/// Binding に対応する実デバイス入力を読み、Player の ActionState に反映する。
	/// </summary>
	/// <param name="binding">適用する入力 Binding。</param>
	void InputSystem::ApplyBinding(const InputBinding& binding)
	{
		if (!IsValidPlayerIndex(binding.playerIndex))
		{
			return;
		}

		PlayerInputState& player = players[binding.playerIndex];
		InputValue value;
		InputDeviceType deviceType = InputDeviceType::None;

		if (binding.deviceType == BindingDeviceType::Keyboard)
		{
			if (binding.valueType == InputValueType::Button)
			{
				value = InputValue::Button(IsKeyboardKeyDown(binding.keyboardButton.key));
			}
			else if (binding.valueType == InputValueType::Axis2D)
			{
				// 4 方向キーから 2D 軸を作る。斜め入力は長さ 1 に正規化する。
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

			deviceType = InputDeviceType::Keyboard;
		}
		else if (binding.deviceType == BindingDeviceType::Gamepad)
		{
			if (binding.valueType == InputValueType::Button)
			{
				value = InputValue::Button(IsGamepadButtonDown(binding.gamepadButton.gamepadIndex, binding.gamepadButton.button));
			}
			else if (binding.valueType == InputValueType::Axis2D)
			{
				value = InputValue::Axis2D(GetGamepadAxis2D(binding.gamepadAxis2D));
			}

			deviceType = InputDeviceType::Gamepad;
		}
		else
		{
			return;
		}

		MergeActionValue(player, binding, value, deviceType);
	}

	/// <summary>
	/// XInput のゲームパッド接続状態と入力値を、今フレーム用にまとめて取得する。
	/// </summary>
	void InputSystem::UpdateGamepadStates()
	{
		for (int gamepadIndex = 0; gamepadIndex < MaxXInputGamepads; ++gamepadIndex)
		{
			XINPUT_STATE state = {};
			const DWORD result = XInputGetState(static_cast<DWORD>(gamepadIndex), &state);
			cachedGamepadConnected[gamepadIndex] = result == ERROR_SUCCESS;
			cachedGamepadStates[gamepadIndex] = cachedGamepadConnected[gamepadIndex] ? state : XINPUT_STATE{};
		}
	}

	/// <summary>
	/// 前フレーム値と今フレーム値を比較し、全 Action の Trigger / Press / Release を確定する。
	/// </summary>
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

				// lastUsedDeviceType は入力が無いフレームでも残すため、入力があった時だけ更新する。
				if (currentActive && action.deviceType != InputDeviceType::None)
				{
					player.lastUsedDeviceType = action.deviceType;
				}
			}
		}
	}

	/// <summary>
	/// Player 番号が InputSystem の管理範囲内か確認する。
	/// </summary>
	/// <param name="playerIndex">確認する Player 番号。</param>
	/// <returns>範囲内なら true。</returns>
	bool InputSystem::IsValidPlayerIndex(int playerIndex)
	{
		return playerIndex >= 0 && playerIndex < MaxPlayers;
	}

	/// <summary>
	/// Win32 API から指定キーボードキーの現在状態を取得する。
	/// </summary>
	/// <param name="key">確認するキーボードキー。</param>
	/// <returns>押されていれば true。</returns>
	bool InputSystem::IsKeyboardKeyDown(KeyboardKey key)
	{
		if (key == KeyboardKey::None)
		{
			return false;
		}

		return (GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;
	}

	/// <summary>
	/// XInput のゲームパッド番号が管理範囲内か確認する。
	/// </summary>
	/// <param name="gamepadIndex">確認するゲームパッド番号。</param>
	/// <returns>範囲内なら true。</returns>
	bool InputSystem::IsValidGamepadIndex(int gamepadIndex)
	{
		return gamepadIndex >= 0 && gamepadIndex < MaxXInputGamepads;
	}

	/// <summary>
	/// 指定ゲームパッドの指定ボタンが押されているか確認する。
	/// </summary>
	/// <param name="gamepadIndex">XInput のゲームパッド番号。</param>
	/// <param name="button">確認するボタン。</param>
	/// <returns>接続されていてボタンが押されていれば true。</returns>
	bool InputSystem::IsGamepadButtonDown(int gamepadIndex, GamepadButton button)
	{
		if (!IsValidGamepadIndex(gamepadIndex) || !cachedGamepadConnected[gamepadIndex])
		{
			return false;
		}

		const WORD xinputButton = ToXInputButton(button);
		const XINPUT_GAMEPAD& gamepad = cachedGamepadStates[gamepadIndex].Gamepad;
		if (button == GamepadButton::LeftTrigger)
		{
			return gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
		}

		if (button == GamepadButton::RightTrigger)
		{
			return gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
		}

		if (xinputButton == 0)
		{
			return false;
		}

		return (gamepad.wButtons & xinputButton) != 0;
	}

	/// <summary>
	/// 指定ゲームパッド Binding から Axis2D 入力を取得する。
	/// </summary>
	/// <param name="binding">ゲームパッド番号と Axis 入力元を持つ Binding。</param>
	/// <returns>接続されていれば Axis2D、未接続なら Zero。</returns>
	Vector2 InputSystem::GetGamepadAxis2D(const GamepadAxis2DBinding& binding)
	{
		if (!IsValidGamepadIndex(binding.gamepadIndex) || !cachedGamepadConnected[binding.gamepadIndex])
		{
			return Vector2::Zero;
		}

		const XINPUT_GAMEPAD& gamepad = cachedGamepadStates[binding.gamepadIndex].Gamepad;
		if (binding.source == GamepadAxis2DSource::DPad)
		{
			(void)gamepad;

			Vector2 axis = Vector2::Zero;
			if (IsGamepadButtonDown(binding.gamepadIndex, binding.negativeXButton)) axis.x -= 1.0f;
			if (IsGamepadButtonDown(binding.gamepadIndex, binding.positiveXButton)) axis.x += 1.0f;
			if (IsGamepadButtonDown(binding.gamepadIndex, binding.negativeYButton)) axis.y -= 1.0f;
			if (IsGamepadButtonDown(binding.gamepadIndex, binding.positiveYButton)) axis.y += 1.0f;

			if (axis.LengthSquared() > 1.0f)
			{
				axis.Normalize();
			}

			return axis;
		}

		const bool useRightStick = binding.source == GamepadAxis2DSource::RightStick;
		const SHORT rawX = useRightStick ? gamepad.sThumbRX : gamepad.sThumbLX;
		const SHORT rawY = useRightStick ? gamepad.sThumbRY : gamepad.sThumbLY;
		const Vector2 rawAxis(NormalizeThumbValue(rawX), NormalizeThumbValue(rawY));

		return ApplyRadialDeadZone(rawAxis, settings.axisDeadZone);
	}

	/// <summary>
	/// 複数 Binding が同じ Action に割り当てられている場合に、入力値を合成する。
	/// </summary>
	/// <param name="player">入力値を反映する PlayerInputState。</param>
	/// <param name="binding">値の反映先 Action を持つ Binding。</param>
	/// <param name="value">合成する入力値。</param>
	/// <param name="deviceType">この入力値を発生させたデバイス種別。</param>
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
			// 複数軸 Binding が重なっても、最終的な移動量が 1 を超えないようにする。
			action.value.axis += value.axis;
			if (action.value.axis.LengthSquared() > 1.0f)
			{
				action.value.axis.Normalize();
			}
			action.value.scalar = action.value.axis.Length();
		}
		else
		{
			// Button は 1 つでも押されていれば押下扱いにする。
			action.value.scalar = std::max(action.value.scalar, value.scalar);
			action.value.axis = Vector2(action.value.scalar, 0.0f);
		}

		// 実際に入力があった Binding だけ、今フレームの発生デバイスとして記録する。
		if (value.IsActive(settings.buttonThreshold))
		{
			action.deviceType = deviceType;
		}
	}
}
