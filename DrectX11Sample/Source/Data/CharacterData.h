#pragma once

#include "Data/AttackData.h"

#include <string>
#include <vector>

/// <summary>
/// キャラクターごとに変わる移動、ジャンプ、重力などの基本パラメータ。
/// </summary>
struct CharacterParameterData
{
	// CharacterData フォルダ名と対応するキャラクター ID。
	std::string characterId = "DebugPlayer";
	// UI やデバッグ表示で使う表示名。
	std::string displayName = "デバッグプレイヤー";

	float forwardWalkSpeed = 0.08f;
	float backwardWalkSpeed = 0.06f;
	float jumpInitialVelocity = 0.32f;
	float frontJumpHorizontalVelocity = 0.10f;
	float backJumpHorizontalVelocity = -0.05f;
	float riseGravityPerFrame = -0.012f;
	float fallGravityPerFrame = -0.020f;
};

/// <summary>
/// キャラクター基本パラメータと、そのキャラクターに割り当てられた技データをまとめる。
/// </summary>
struct CharacterData
{
	CharacterParameterData parameter;
	std::vector<CharacterAssignedAttackData> attacks;
};
