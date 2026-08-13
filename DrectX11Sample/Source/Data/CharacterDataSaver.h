#pragma once

#include "Data/CharacterData.h"

#include <string>

class CharacterDataSaver
{
public:
	/// <summary>
	/// CharacterData を Loader 互換の Parameter.json / AttackList.json として保存する。
	/// </summary>
	/// <param name="characterFolderPath">保存先キャラクターフォルダ。例: assets/CharacterData/CharacterSlot00。</param>
	/// <param name="parameter">保存するキャラクター基本情報。編集画面では主に characterName を変更する。</param>
	/// <param name="attackSlots">キャラクターに割り当てる技スロット情報。</param>
	/// <returns>保存できた場合は true。</returns>
	static bool SaveCharacterData(
		const std::string& characterFolderPath,
		const CharacterParameterData& parameter,
		const std::vector<CharacterAttackSlotData>& attackSlots);
};
