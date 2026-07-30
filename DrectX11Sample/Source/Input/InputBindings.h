#pragma once

#include "Input/InputTypes.h"

#include <vector>

namespace Input
{
	// Win32 の仮想キーコードを、このプロジェクト内で扱いやすい名前にしたもの。
	// JSON 化する時は、この enum と文字列を相互変換する。
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
		Y = 'Y',

		Down = 0x28,
		Enter = 0x0D,
		Escape = 0x1B,
		Left = 0x25,
		Right = 0x27,
		Space = 0x20,
		Up = 0x26
	};

	// XInput のボタンをプロジェクト内の Binding 用に表す。
	// JSON 化する時は KeyboardKey と同じく、文字列との相互変換を追加する。
	enum class GamepadButton : uint16_t
	{
		None = 0,

		DPadUp,
		DPadDown,
		DPadLeft,
		DPadRight,
		Start,
		Back,
		LeftThumb,
		RightThumb,
		LeftShoulder,
		RightShoulder,
		A,
		B,
		X,
		Y
	};

	// 2D Axis として読むゲームパッド入力。
	// DPad は上下左右ボタンから Axis2D を作り、Stick はアナログ値を使う。
	enum class GamepadAxis2DSource : uint8_t
	{
		LeftStick,
		RightStick,
		DPad
	};

	// Binding がどの入力デバイスを読むかを表す。
	// 実際にどの Player へ渡すかは InputBinding::playerIndex で決める。
	enum class BindingDeviceType : uint8_t
	{
		Keyboard,
		Gamepad
	};

	struct KeyboardButtonBinding
	{
		// Button Action で見る単一キー。
		KeyboardKey key = KeyboardKey::None;
	};

	struct KeyboardAxis2DBinding
	{
		// 2D 入力を作る 4 方向キー。
		// 左右や上下を同時に押すと打ち消し、斜めは InputSystem 側で長さ 1 以下に収める。
		KeyboardKey negativeX = KeyboardKey::None;
		KeyboardKey positiveX = KeyboardKey::None;
		KeyboardKey negativeY = KeyboardKey::None;
		KeyboardKey positiveY = KeyboardKey::None;
	};

	struct GamepadButtonBinding
	{
		// XInput のコントローラー番号。0 が最初に認識されたゲームパッド。
		int gamepadIndex = 0;

		// Button Action で見る単一ボタン。
		GamepadButton button = GamepadButton::None;
	};

	struct GamepadAxis2DBinding
	{
		// XInput のコントローラー番号。1P/2P の割り当て変更時はここを書き換える。
		int gamepadIndex = 0;

		// 左スティック、右スティック、十字キーのどれから Axis2D を作るか。
		GamepadAxis2DSource source = GamepadAxis2DSource::LeftStick;
	};

	struct InputBinding
	{
		// どの ActionMap 中で有効になる Binding か。
		InputActionMapId actionMap = InputActionMapId::Gameplay;

		// 具体キー入力をどの抽象 Action へ反映するか。
		InputActionId action = InputActionId::Move;

		// Button / Axis など、Action 値の型。
		InputValueType valueType = InputValueType::Button;

		// 入力元デバイス。
		BindingDeviceType deviceType = BindingDeviceType::Keyboard;

		// どの PlayerInputState に反映するか。
		int playerIndex = 0;

		// deviceType / valueType に応じて使う具体 Binding。
		KeyboardButtonBinding keyboardButton;
		KeyboardAxis2DBinding keyboardAxis2D;
		GamepadButtonBinding gamepadButton;
		GamepadAxis2DBinding gamepadAxis2D;
	};

	struct InputSettings
	{
		// scalar / axis を「入力あり」とみなすしきい値。
		float buttonThreshold = 0.5f;

		// ゲームパッドのスティックを無入力扱いにする中央付近のしきい値。
		float axisDeadZone = 0.25f;
	};

	// 初期 Binding をコードから作る。将来 JSON 読み込みが成功した場合は差し替える。
	std::vector<InputBinding> CreateDefaultInputBindings();
}
