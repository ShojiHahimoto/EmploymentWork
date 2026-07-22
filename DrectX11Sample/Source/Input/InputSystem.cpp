#include "Input/InputSystem.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>

using namespace DirectX::SimpleMath;

namespace Input
{
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
	/// 初期状態で使うキーボード Binding を作る。
	/// </summary>
	/// <returns>Gameplay と UI の既定 Binding 配列。</returns>
	std::vector<InputBinding> CreateDefaultInputBindings()
	{
		std::vector<InputBinding> defaultBindings;

		// Button Action 用の Binding を 1 件追加する小さな補助。
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

		// WASD や矢印キーなど、4 キーから Axis2D Action を作る Binding を追加する。
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

		// Gameplay はキャラクター操作用。現状は Player 0 のキーボードだけに割り当てる。
		addKeyboardAxis2D(InputActionMapId::Gameplay, InputActionId::Move, KeyboardKey::A, KeyboardKey::D, KeyboardKey::S, KeyboardKey::W);
		addKeyboardButton(InputActionMapId::Gameplay, InputActionId::LightAttack, KeyboardKey::J);
		addKeyboardButton(InputActionMapId::Gameplay, InputActionId::MediumAttack, KeyboardKey::K);
		addKeyboardButton(InputActionMapId::Gameplay, InputActionId::HeavyAttack, KeyboardKey::L);
		addKeyboardButton(InputActionMapId::Gameplay, InputActionId::Guard, KeyboardKey::I);
		addKeyboardButton(InputActionMapId::Gameplay, InputActionId::Pause, KeyboardKey::Escape);

		// UI はメニュー操作用。Gameplay と同時には有効にしない。
		addKeyboardAxis2D(InputActionMapId::UI, InputActionId::Move, KeyboardKey::Left, KeyboardKey::Right, KeyboardKey::Down, KeyboardKey::Up);
		addKeyboardButton(InputActionMapId::UI, InputActionId::Submit, KeyboardKey::Enter);
		addKeyboardButton(InputActionMapId::UI, InputActionId::Submit, KeyboardKey::Space);
		addKeyboardButton(InputActionMapId::UI, InputActionId::Cancel, KeyboardKey::Escape);

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
	}

	/// <summary>
	/// 1 フレーム分の入力を取得し、ActionState の Trigger / Press / Release を確定する。
	/// </summary>
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

		MergeActionValue(player, binding, value, InputDeviceType::Keyboard);
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
