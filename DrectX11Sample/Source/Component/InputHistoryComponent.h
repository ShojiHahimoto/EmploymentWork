#pragma once

#include "Component/Component.h"
#include "Input/InputTypes.h"

#include <array>

struct InputHistoryComponent : public Component
{
	// 現段階では今フレーム分だけを保存する。
	// 将来は HistoryFrameCount を増やし、ring buffer として数十フレーム分を保持する。
	static constexpr int HistoryFrameCount = 1;

	std::array<Input::PlayerInputState, HistoryFrameCount> frames = {};
	int latestFrameIndex = 0;
};
