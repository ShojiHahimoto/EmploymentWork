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

	// Button 入力を 0/1 の scalar に変換する。
	InputValue InputValue::Button(bool pressed)
	{
		InputValue value;
		value.type = InputValueType::Button;
		value.scalar = pressed ? 1.0f : 0.0f;
		value.axis = Vector2(value.scalar, 0.0f);
		return value;
	}

	// 1 軸入力を scalar と axis.x に保持する。
	InputValue InputValue::Axis1D(float value)
	{
		InputValue inputValue;
		inputValue.type = InputValueType::Axis1D;
		inputValue.scalar = value;
		inputValue.axis = Vector2(value, 0.0f);
		return inputValue;
	}

	// 2 軸入力を axis に保持し、scalar には強さを入れる。
	InputValue InputValue::Axis2D(const Vector2& value)
	{
		InputValue inputValue;
		inputValue.type = InputValueType::Axis2D;
		inputValue.scalar = value.Length();
		inputValue.axis = value;
		return inputValue;
	}

	// 入力値がしきい値を超えているか判定する。
	// Button / Axis1D は scalar、Axis2D はベクトル長を使う。
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

	// 初期状態で使うキーボード Binding を作る。
	// 将来 JSON 読み込みを入れた場合も、失敗時の fallback として使える。
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
		addKeyboardButton(InputActionMapId::Gameplay, InputActionId::Jump, KeyboardKey::Space);
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

	// 入力設定、初期 Binding、Player 割り当てを初期化する。
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

	// フレーム入力を確定する。
	// previous 退避 -> current クリア -> Binding 適用 -> Trigger/Press/Release 確定の順で行う。
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

	// 入力状態を破棄する。Scene 終了ではなく Game 終了時に呼ぶ。
	void InputSystem::Shutdown()
	{
		bindings.clear();
		for (PlayerInputState& player : players)
		{
			player = PlayerInputState{};
		}
		currentActionMap = InputActionMapId::Gameplay;
	}

	// 有効な ActionMap を切り替える。
	// 切り替え直後に前マップの入力で Trigger/Release が出ないよう、全 Action をリセットする。
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

	// 現在有効な ActionMap を返す。
	InputActionMapId InputSystem::GetActionMap()
	{
		return currentActionMap;
	}

	// 指定 Player / Action の状態を返す。
	// 範囲外アクセスでは emptyActionState を返し、呼び出し側を壊さない。
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

	// 指定 Player の全入力状態を返す。
	// 将来、バトル用入力履歴はこの戻り値をフレームごとにコピーすればよい。
	const PlayerInputState& InputSystem::GetPlayerInputState(int playerIndex)
	{
		if (!IsValidPlayerIndex(playerIndex))
		{
			return players[0];
		}

		return players[playerIndex];
	}

	// 指定 Player が最後に使ったデバイス種別を返す。
	InputDeviceType InputSystem::GetLastUsedDeviceType(int playerIndex)
	{
		if (!IsValidPlayerIndex(playerIndex))
		{
			return InputDeviceType::None;
		}

		return players[playerIndex].lastUsedDeviceType;
	}

	// Binding 一式を差し替える。
	// JSON キーコンフィグ読み込み後は、この API に読み込み結果を渡す。
	void InputSystem::SetBindings(const std::vector<InputBinding>& newBindings)
	{
		bindings = newBindings;
	}

	// 現在の Binding 一覧を返す。デバッグ表示や保存処理で使う。
	const std::vector<InputBinding>& InputSystem::GetBindings()
	{
		return bindings;
	}

	// 入力しきい値などの設定を差し替える。
	void InputSystem::SetSettings(const InputSettings& newSettings)
	{
		settings = newSettings;
	}

	// 現在の入力設定を返す。
	const InputSettings& InputSystem::GetSettings()
	{
		return settings;
	}

	// 今フレーム値を前フレーム値へ退避し、フレーム結果フラグを一旦消す。
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

	// Binding を適用する前に、今フレーム値だけを初期化する。
	// previousValue は Trigger/Release 判定に使うため残す。
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

	// Binding に対応する実デバイス入力を読み、ActionState に反映する。
	// 今回は Keyboard のみ処理し、Gamepad は構造だけ残している。
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

	// 前フレーム値と今フレーム値を比較して、Trigger / Press / Release を確定する。
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

	// Player 配列の範囲内か確認する。
	bool InputSystem::IsValidPlayerIndex(int playerIndex)
	{
		return playerIndex >= 0 && playerIndex < MaxPlayers;
	}

	// Win32 の現在キー状態を読む。
	// InputSystem::Update 内でだけ呼び、他 System が直接キー状態を読まないようにする。
	bool InputSystem::IsKeyboardKeyDown(KeyboardKey key)
	{
		if (key == KeyboardKey::None)
		{
			return false;
		}

		return (GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;
	}

	// 複数 Binding が同じ Action に割り当てられている場合、値を合成する。
	// 例: Submit に Enter と Space の両方を割り当てる。
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
