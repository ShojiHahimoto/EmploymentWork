#pragma once

#include "Data/MotionData.h"

#include <string>

class MotionDataLoader
{
public:
	/// <summary>
	/// 指定 MotionData ID に対応する JSON を読み込む。
	/// </summary>
	/// <param name="motionDataId">assets/MotionData 配下のモーション ID。</param>
	/// <param name="outMotionData">読み込んだモーションデータの書き込み先。</param>
	/// <returns>読み込みに成功した場合は true。</returns>
	static bool LoadMotionData(const std::string& motionDataId, MotionData& outMotionData);
};
