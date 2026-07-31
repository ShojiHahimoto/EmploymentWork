#pragma once

#include "Component/Component.h"

/// <summary>
/// 対戦中のラウンドタイマーを保持するデータ専用 Component。
/// </summary>
struct BattleTimerComponent : public Component
{
	// 固定 60fps 前提の総フレーム数。
	int totalFrames = 99 * 60;

	// 現在残っているフレーム数。BattleResultSystem が毎フレーム減算する。
	int remainingFrames = 99 * 60;

	// false の間はカウントダウンしない。演出待ちなどを後で挟むための余地。
	bool isRunning = true;
};
