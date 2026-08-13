#pragma once

#include "Data/AttackData.h"

#include <string>

class AttackDataSaver
{
public:
	/// <summary>
	/// AttackData を assets/AttackData 配下の JSON として保存する。
	/// </summary>
	/// <param name="attackDataId">assets/AttackData から見た拡張子なしの保存 ID。</param>
	/// <param name="attackData">保存する技データ。</param>
	/// <returns>保存できた場合は true。</returns>
	static bool SaveAttackData(const std::string& attackDataId, const AttackData& attackData);
};
