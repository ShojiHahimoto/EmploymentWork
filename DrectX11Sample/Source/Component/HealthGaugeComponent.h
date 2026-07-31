#pragma once

#include "Component/Component.h"

/// <summary>
/// HPバー表示に必要な割合と表示対象 Player を保持するデータ専用 Component。
/// </summary>
struct HealthGaugeComponent : public Component
{
	// World が保持する BattlePlayerId の index。0 が Player1、1 が Player2。
	int targetPlayerIndex = 0;

	// 現在 HP に即時追従するバーの割合。
	float healthRatio = 1.0f;

	// ヒットスタン中だけ古い HP 量を維持する演出用バーの割合。
	float damageRatio = 1.0f;

	// 初回更新時に現在 HP から両バーを初期化するためのフラグ。
	bool initialized = false;

	// Transform の localPosition.xy を左上座標、localScale.xy をピクセルサイズとして扱う。
	float maxWidth = 360.0f;
	float height = 24.0f;
	float top = 32.0f;
	float horizontalMargin = 40.0f;
};
