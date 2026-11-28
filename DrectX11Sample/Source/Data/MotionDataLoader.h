#pragma once

#include "Data/MotionData.h"

#include <string>

class MotionDataLoader
{
public:
	static bool LoadMotionData(const std::string& motionDataId, MotionData& outMotionData);
};
