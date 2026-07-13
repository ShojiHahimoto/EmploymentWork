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

		Down = 0x28,
		Enter = 0x0D,
		Escape = 0x1B,
		Left = 0x25,
		Right = 0x27,
		Space = 0x20,
		Up = 0x26
	};

	// Binding がどの入力デバイスを読むかを表す。
	// Gamepad は今回未実装だが、Binding 構造を崩さず追加できるように残す。
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
		// 同時押し時は InputSystem 側で正規化して長さ 1 以下に収める。
		KeyboardKey negativeX = KeyboardKey::None;
		KeyboardKey positiveX = KeyboardKey::None;
		KeyboardKey negativeY = KeyboardKey::None;
		KeyboardKey positiveY = KeyboardKey::None;
	};

	struct InputBinding
	{
		// どの ActionMap 中で有効になる Binding か。
		InputActionMapId actionMap = InputActionMapId::Gameplay;

		// 具体キー入力をどの抽象 Action へ反映するか。
		InputActionId action = InputActionId::Move;

		// Button / Axis など、Action 値の型。
		InputValueType valueType = InputValueType::Button;

		// 入力元デバイス。今回は Keyboard のみ処理する。
		BindingDeviceType deviceType = BindingDeviceType::Keyboard;

		// どの PlayerInputState に反映するか。
		int playerIndex = 0;

		// deviceType / valueType に応じて使う具体 Binding。
		KeyboardButtonBinding keyboardButton;
		KeyboardAxis2DBinding keyboardAxis2D;
	};

	struct InputSettings
	{
		// scalar / axis を「入力あり」とみなすしきい値。
		float buttonThreshold = 0.5f;

		// ゲームパッド追加時のスティックしきい値。今回は保持のみ。
		float axisDeadZone = 0.25f;
	};

	// 初期 Binding をコードから作る。将来 JSON 読み込みが成功した場合は差し替える。
	std::vector<InputBinding> CreateDefaultInputBindings();
}
