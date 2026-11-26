#pragma once

#include "Data/AttackData.h"

#include <SimpleMath.h>

#include <string>
#include <vector>

/// <summary>
/// キャラクターごとの PushBox / HurtBox 初期形状を保持する。
/// </summary>
struct CharacterBoxParameterData
{
	// キャラクター座標を基準にした中心オフセット。
	DirectX::SimpleMath::Vector2 offset = DirectX::SimpleMath::Vector2::Zero;

	// 2D AABB の横幅と高さ。
	DirectX::SimpleMath::Vector2 size = DirectX::SimpleMath::Vector2::Zero;
};

/// <summary>
/// キャラクターごとに変わる移動、ジャンプ、重力、見た目、当たり判定初期値などの基本パラメータ。
/// </summary>
struct CharacterParameterData
{
	// CharacterData フォルダ名と対応するキャラクター ID。
	std::string characterId = "CharacterSlot00";
	// UI やデバッグ表示で使うキャラクター名。
	std::string characterName = "デバッグプレイヤー";

	float forwardWalkSpeed = 0.14f;
	float backwardWalkSpeed = 0.12f;
	float jumpInitialVelocity = 1.02f;
	float frontJumpHorizontalVelocity = 0.15f;
	float backJumpHorizontalVelocity = -0.13f;
	float riseGravityPerFrame = -0.045f;
	float fallGravityPerFrame = -0.040f;
	int maxHp = 10000;
	DirectX::SimpleMath::Vector3 modelScale = DirectX::SimpleMath::Vector3(0.05f, 0.05f, 0.05f);
	CharacterBoxParameterData pushBox = { DirectX::SimpleMath::Vector2(-0.0f, 4.5f), DirectX::SimpleMath::Vector2(4.0f, 9.5f) };
	CharacterBoxParameterData hurtBox = { DirectX::SimpleMath::Vector2(-0.0f, 4.5f), DirectX::SimpleMath::Vector2(3.5f, 9.0f) };
};

/// <summary>
/// キャラクター基本パラメータと、そのキャラクターに割り当てられた技データをまとめる。
/// </summary>
struct CharacterData
{
	CharacterParameterData parameter;
	std::vector<CharacterAssignedAttackData> attacks;
};
