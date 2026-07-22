#pragma once

#include "Component/Component.h"

#include <array>

struct InputButtonHistoryState
{
	// InputSystem が判定済みの結果を、履歴として保存する。
	// ここで Trigger / Press / Release を再計算しない。
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
