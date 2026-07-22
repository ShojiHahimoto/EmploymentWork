#pragma once

#include <SimpleMath.h>

#include <array>
#include <cstdint>

namespace Input
{
	// 1P/2Pまでを想定する。
	// 入力履歴やコマンド判定側も、この上限を基準に配列を持てるようにする。
	constexpr int MaxPlayers = 2;

	// ゲーム全体で現在有効な入力マップ。
	// プレイヤーごとに別マップは持たず、UI中なら全員UI操作だけを見る。
	enum class InputActionMapId : uint8_t
	{
		Gameplay,
		UI,
		Count
	};

	// System側が参照する抽象アクション。
	// キーボードやゲームパッドの具体入力はInputBinding側でこの値へ丸め込む。
	enum class InputActionId : uint8_t
	{
		Move,
		Jump,
		LightAttack,
		MediumAttack,
		HeavyAttack,
		Guard,
		Pause,
		Submit,
		Cancel,
		Count
	};

	// 最後に入力されたデバイス種別を残し、操作説明UIの表示切り替えなどに使う。
	enum class InputDeviceType : uint8_t
	{
		None,
		Keyboard,
		Gamepad
	};

	// Actionが返す値の形。移動は Axis2D、攻撃やガードはButtonを想定する。
	// バトル中のジャンプは専用ボタンではなく、Move の 7 / 8 / 9 方向から作る。
	enum class InputValueType : uint8_t
	{
		Button,
		Axis1D,
		Axis2D
	};

	struct InputValue
	{
		// scalar は Button / Axis1D 用、axis は Axis2D 用。
		// どちらの型でも IsActive で入力有無を判定できるようにする。
		InputValueType type = InputValueType::Button;
		float scalar = 0.0f;
		DirectX::SimpleMath::Vector2 axis = DirectX::SimpleMath::Vector2::Zero;

		// 各入力型の値を作る補助関数。
		static InputValue Button(bool pressed);
		static InputValue Axis1D(float value);
		static InputValue Axis2D(const DirectX::SimpleMath::Vector2& value);

		// Trigger / Press / Release の判定用に、値が入力中とみなせるか返す。
		bool IsActive(float threshold = 0.5f) const;
	};

	struct InputActionState
	{
		// value は今フレーム、previousValue は前フレームの確定値。
		// フレーム境界でのみ更新し、同一フレーム内では結果を固定する。
		InputValue value;
		InputValue previousValue;

		// Trigger: previous frame is not active, current frame is active.
		bool trigger = false;
		// Press: current frame is active.
		bool press = false;
		// Release: previous frame is active, current frame is not active.
		bool release = false;

		// この Action の今フレーム値を発生させたデバイス。
		// 入力が無いフレームは None に戻る。
		InputDeviceType deviceType = InputDeviceType::None;
	};

	using ActionStateArray = std::array<InputActionState, static_cast<size_t>(InputActionId::Count)>;

	struct PlayerInputState
	{
		// Player ごとの全 Action 状態。
		ActionStateArray actions = {};

		// 最後に有効入力を出したデバイス。入力が無いフレームでも保持する。
		InputDeviceType lastUsedDeviceType = InputDeviceType::None;

		// 現状は Player 0 のみ true。将来のキー割り当て変更で使う。
		bool keyboardAssigned = false;

		// XInput 等を追加する時のゲームパッド番号。未割り当ては -1。
		int gamepadIndex = -1;
	};
}
