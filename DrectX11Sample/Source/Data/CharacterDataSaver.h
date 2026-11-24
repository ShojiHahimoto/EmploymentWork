#pragma once

#include "Data/CharacterData.h"

#include <string>

class CharacterDataSaver
{
public:
	static bool SaveCharacterData(
		const std::string& characterFolderPath,
		const CharacterParameterData& parameter,
		const std::vector<CharacterAttackSlotData>& attackSlots);
};
