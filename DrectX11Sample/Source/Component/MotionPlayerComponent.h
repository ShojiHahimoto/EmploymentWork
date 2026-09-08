#pragma once

#include "Component/Component.h"

#include "Component/StateComponent.h"

#include <string>

/// <summary>
/// GameObject が現在再生している MotionData と再生フレームを保持する。
/// </summary>
struct MotionPlayerComponent : public Component
{
	// MotionDataManager から取得するモーション ID。空文字ならモーション再生しない。
	std::string motionDataId;

	// 現在再生中の 0 始まりフレーム。
	int currentFrame = 0;

	// true の場合、MotionData::totalFrames 到達後に 0 へ戻す。
	bool looping = true;

	// false の場合、姿勢は bind pose のまま更新する。
	bool playing = true;

	// State / AttackData から自動設定されたモーションかどうか。
	bool stateDriven = false;

	// stateDriven 再生の前回 ActionState。状態が変わったら再生フレームをリセットする。
	PlayerActionState boundActionState = PlayerActionState::Idle;

	// stateDriven 再生の前回攻撃スロット。技が変わったら再生フレームをリセットする。
	std::string boundAttackSlotId;
};
