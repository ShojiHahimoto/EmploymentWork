#pragma once

#include "Data/EffectData.h"

#include <memory>
#include <string>
#include <unordered_map>

class EffectDataLoader
{
public:
	static bool LoadEffectData(const std::string& effectDataId, EffectData& outEffectData);
};

class EffectDataManager
{
public:
	static const EffectData* GetEffectData(const std::string& effectDataId);
};
