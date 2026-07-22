#pragma once

#include "Component/Component.h"

#include <array>

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
	// テンキー表記の方向入力。未入力は 5。
	// 7 8 9
	// 4 5 6
	// 1 2 3
	int direction = 5;

	InputButtonHistoryState lightAttack;
	InputButtonHistoryState mediumAttack;
	InputButtonHistoryState heavyAttack;
	// 専用ボタンではなく、direction が 7 / 8 / 9 かどうかから作るジャンプ入力。
	InputButtonHistoryState jump;
	InputButtonHistoryState guard;
};

struct InputHistoryComponent : public Component
{
	// 現段階では今フレーム分だけを保存する。
	// 将来は HistoryFrameCount を増やし、ring buffer として数十フレーム分を保持する。
	static constexpr int HistoryFrameCount = 1;

	std::array<InputHistoryFrame, HistoryFrameCount> frames = {};
	int latestFrameIndex = 0;
};
