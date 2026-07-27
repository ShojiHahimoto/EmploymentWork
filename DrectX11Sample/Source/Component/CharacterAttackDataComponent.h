#pragma once

#include "Component/Component.h"
#include "Data/AttackData.h"

#include <vector>

/// <summary>
/// 対戦中の System が参照する、キャラクターに割り当て済みの技データ。
/// </summary>
struct CharacterAttackDataComponent : public Component
{
	// slotId と読み込み済み AttackData の対応。System は JSON ではなくここを読む。
	std::vector<CharacterAssignedAttackData> attacks;
};
