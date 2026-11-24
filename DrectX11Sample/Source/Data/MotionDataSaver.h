#pragma once

#include "Data/MotionData.h"

#include <string>

class MotionDataSaver
{
public:
	static bool SaveMotionData(const std::string& motionDataId, const MotionData& motionData);
};
