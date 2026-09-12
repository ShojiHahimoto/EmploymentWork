#pragma once

#include "Data/AttackData.h"

#include <string>

enum class CustomizeMode
{
	MainMenu,
	AttackCategorySelect,
	AttackSlotSelect,
	AttackEditor,
	MotionEditor,
	CommonMotionSelect,
	CharacterSlotSelect,
	CharacterEditor,
	AttackPicker
};

enum class CustomizeAttackCategory
{
	Ground,
	Air,
	Special
};

enum class CustomizeCharacterAttackSlotGroup
{
	Ground,
	Air,
	Special
};

/// <summary>
/// 技スロット一覧に表示する、保存済み AttackData の概要を保持する。
/// </summary>
struct CustomizeAttackSlotSummary
{
	bool hasSavedData = false;
	std::string displayName;
};

/// <summary>
/// キャラクタースロット一覧に表示する、保存済み CharacterData の概要を保持する。
/// </summary>
struct CustomizeCharacterSlotSummary
{
	bool hasSavedData = false;
	std::string characterName;
};

/// <summary>
/// キャラクター編集画面で一時的に保持する技スロット割り当て。
/// </summary>
struct CustomizeCharacterAttackSlotDraft
{
	std::string slotId;
	std::string attackDataId;
	std::string attackDisplayName;
	AttackSlotType slotType = AttackSlotType::Normal;
	AttackButtonId button = AttackButtonId::None;
	AttackUsableState slotUsableState = AttackUsableState::Ground;
};
