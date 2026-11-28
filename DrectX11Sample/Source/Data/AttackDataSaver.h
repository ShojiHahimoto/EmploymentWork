#pragma once

#include "Data/AttackData.h"

#include <string>

class AttackDataSaver
{
public:
	static bool SaveAttackData(const std::string& attackDataId, const AttackData& attackData);
};
