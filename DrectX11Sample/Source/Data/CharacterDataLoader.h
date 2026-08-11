#pragma once

#include "Data/CharacterData.h"

#include <string>

class CharacterDataLoader
{
public:
	/// <summary>
	/// キャラクターフォルダから基本パラメータと割り当て技データを読み込む。
	/// </summary>
	/// <param name="characterFolderPath">CharacterData 配下の対象キャラクターフォルダ。</param>
	/// <param name="outCharacterData">読み込んだキャラクターデータの書き込み先。</param>
	/// <returns>必要なファイルを読み込めた場合は true。不足時もデフォルト値は保持する。</returns>
	static bool LoadCharacterData(const std::string& characterFolderPath, CharacterData& outCharacterData);

	/// <summary>
	/// 指定 AttackData ID に対応する技 JSON を読み込む。
	/// </summary>
	/// <param name="attackDataId">assets/AttackData 配下の技 ID。例: debug_punch / Ground/slot_00。</param>
	/// <param name="outAttackData">読み込んだ技データの書き込み先。</param>
	/// <returns>読み込みに成功した場合は true。</returns>
	static bool LoadAttackData(const std::string& attackDataId, AttackData& outAttackData);
};
