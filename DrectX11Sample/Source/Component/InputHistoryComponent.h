#pragma once

#include "Component/Component.h"
#include "Component/StateComponent.h"

#include <array>
#include <cstdint>

namespace InputHistoryAttackMask
{
	constexpr uint32_t AttackA = 1u << 0;
	constexpr uint32_t AttackB = 1u << 1;
	constexpr uint32_t AttackX = 1u << 2;
	constexpr uint32_t AttackY = 1u << 3;
}

struct InputButtonHistoryState
{
	// ボタン相当の入力結果を、履歴として保存する。
	// 攻撃やガードは InputSystem の結果をコピーし、ジャンプは 7 / 8 / 9 方向から作る。
	bool trigger = false;
	bool press = false;
	bool release = false;
};

struct InputHistoryFrame
{
	// この履歴が何フレーム目に作られたかを表す通し番号。
	// コマンドバッファの有効期限や入力表示の経過フレーム計算に使う。
	int frameNumber = 0;

	// テンキー表記の方向入力。未入力は 5。
	// 7 8 9
	// 4 5 6
	// 1 2 3
	int direction = 5;

	// コマンド判定は入力された瞬間の向きで相対方向へ変換するため、履歴側にも向きを残す。
	FacingDirection facingDirection = FacingDirection::Right;

	InputButtonHistoryState attackA;
	InputButtonHistoryState attackB;
	InputButtonHistoryState attackX;
	InputButtonHistoryState attackY;

	// 攻撃ボタン群をまとめて扱うためのビットマスク。
	// 個別状態は UI 表示や細かい判定用、mask はコマンド検索用に使う。
	uint32_t attackTriggerMask = 0;
	uint32_t attackPressMask = 0;
	uint32_t attackReleaseMask = 0;

	// 専用ボタンではなく、direction が 7 / 8 / 9 かどうかから作るジャンプ入力。
	InputButtonHistoryState jump;
	InputButtonHistoryState guard;

	// 入力表示用。現段階では「方向と攻撃 Press の組み合わせ」が前フレームと同じかを保存する。
	bool sameAsPrevious = false;
};

struct InputDisplayHistoryEntry
{
	// 入力表示に出すテンキー方向。
	int direction = 5;

	// 入力表示に出す、押されている攻撃ボタン群。
	uint32_t attackPressMask = 0;

	// 同じ direction + attackPressMask が継続しているフレーム数。
	int holdFrames = 0;
};

struct InputHistoryComponent : public Component
{
	// コマンド入力を過去 15F まで遡れるよう、ring buffer として履歴を保持する。
	static constexpr int HistoryFrameCount = 15;

	// 画面表示用の圧縮履歴。最新入力を index 0 に置き、古いものほど後ろへ流す。
	static constexpr int DisplayHistoryEntryCount = 15;
	static constexpr int MaxDisplayHoldFrames = 99;

	std::array<InputHistoryFrame, HistoryFrameCount> frames = {};
	int latestFrameIndex = -1;
	int storedFrameCount = 0;
	int nextFrameNumber = 0;

	std::array<InputDisplayHistoryEntry, DisplayHistoryEntryCount> displayEntries = {};
	int displayEntryCount = 0;
};
