#pragma once

#include "Data/MotionData.h"

#include <string>

class MotionDataSaver
{
public:
	/// <summary>
	/// MotionData を assets/MotionData 配下の JSON として保存する。
	/// </summary>
	/// <param name="motionDataId">assets/MotionData から見た拡張子なしの保存 ID。</param>
	/// <param name="motionData">保存するモーションデータ。</param>
	/// <returns>保存に成功した場合は true。</returns>
	static bool SaveMotionData(const std::string& motionDataId, const MotionData& motionData);
};
