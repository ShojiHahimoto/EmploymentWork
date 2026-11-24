#pragma once

#include "Data/CharacterData.h"

#include <string>

class CharacterDataLoader
{
public:
	static bool LoadCharacterData(const std::string& characterFolderPath, CharacterData& outCharacterData);

	static bool LoadAttackData(const std::string& attackDataId, AttackData& outAttackData);
};
