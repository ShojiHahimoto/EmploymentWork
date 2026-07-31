#pragma once

#include "Component/Component.h"

/// <summary>
/// 対戦中の体力を保持するデータ専用 Component。
/// </summary>
struct HealthComponent : public Component
{
	// キャラクターデータから Spawn 時にコピーする最大 HP。
	int maxHp = 100;

	// 現在 HP。HitResolveSystem が攻撃ヒット確定時に減算し、BattleResultSystem が勝敗判定に使う。
	int currentHp = 100;
};
