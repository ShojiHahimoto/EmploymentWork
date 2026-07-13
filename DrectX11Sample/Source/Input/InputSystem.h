#pragma once

#include "Input/InputBindings.h"

#include <array>
#include <vector>

namespace Input
{
	class InputSystem
	{
	public:
		// 初期 Binding と Player 割り当てを作る。
		static void Initialize();

		// 1 フレーム分の入力を確定する。Game::Update の先頭で 1 回だけ呼ぶ。
		static void Update();

		// 入力状態と Binding を破棄し、次回 Init 前の状態に戻す。
		static void Shutdown();

		// ゲーム全体で有効な ActionMap を切り替える。
		static void SetActionMap(InputActionMapId actionMap);
		static InputActionMapId GetActionMap();

		// 指定 Player / Action の確定済み状態を取得する。
		static const InputActionState& GetActionState(int playerIndex, InputActionId action);

		// Player ごとの全入力状態をまとめて取得する。履歴保存側はこれをコピーできる。
		static const PlayerInputState& GetPlayerInputState(int playerIndex);

		// 操作説明 UI などが、直近入力デバイスを確認するための API。
		static InputDeviceType GetLastUsedDeviceType(int playerIndex);

		// キーコンフィグ読み込み後に Binding 一式を差し替える。
		static void SetBindings(const std::vector<InputBinding>& newBindings);
		static const std::vector<InputBinding>& GetBindings();

		// デッドゾーンや入力しきい値を差し替える。
		static void SetSettings(const InputSettings& newSettings);
		static const InputSettings& GetSettings();

	private:
		// 現在有効な ActionMap。プレイヤーごとには分けない。
		static InputActionMapId currentActionMap;

		// 入力判定の調整値。
		static InputSettings settings;

		// キーやゲームパッド入力を Action へ変換する設定。
		static std::vector<InputBinding> bindings;

		// Player ごとの現在/前フレーム入力状態。
		static std::array<PlayerInputState, MaxPlayers> players;

		// 不正な参照要求に返す空状態。
		static InputActionState emptyActionState;

		// 今フレームの value を previousValue へ退避する。
		static void UpdatePreviousValues();

		// Binding 適用前に今フレーム値を空に戻す。
		static void ClearCurrentValues();

		// 1 つの Binding を読み、対応する Action に反映する。
		static void ApplyBinding(const InputBinding& binding);

		// previousValue と value から Trigger / Press / Release を確定する。
		static void FinalizeActionStates();

		static bool IsValidPlayerIndex(int playerIndex);
		static bool IsKeyboardKeyDown(KeyboardKey key);

		// 同じ Action に複数 Binding がある場合、入力値を 1 つの ActionState に合成する。
		static void MergeActionValue(PlayerInputState& player, const InputBinding& binding, const InputValue& value, InputDeviceType deviceType);
	};
}
