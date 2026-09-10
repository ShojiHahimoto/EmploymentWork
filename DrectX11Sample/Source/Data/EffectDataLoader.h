#pragma once

#include "Data/EffectData.h"

#include <memory>
#include <string>
#include <unordered_map>

class EffectDataLoader
{
public:
	/// <summary>
	/// 指定 EffectData ID に対応する JSON を読み込む。
	/// </summary>
	/// <param name="effectDataId">assets/EffectData 配下のエフェクト ID。</param>
	/// <param name="outEffectData">読み込んだエフェクトデータの書き込み先。</param>
	/// <returns>読み込みに成功した場合は true。</returns>
	static bool LoadEffectData(const std::string& effectDataId, EffectData& outEffectData);
};

class EffectDataManager
{
public:
	/// <summary>
	/// 指定 EffectData ID をキャッシュへ読み込む。
	/// </summary>
	/// <param name="effectDataId">読み込む EffectData ID。</param>
	/// <returns>読み込み済み、または読み込み成功なら true。</returns>
	static bool LoadEffectData(const std::string& effectDataId);

	/// <summary>
	/// 読み込み済みの EffectData を取得する。
	/// </summary>
	/// <param name="effectDataId">取得する EffectData ID。</param>
	/// <returns>見つかった EffectData。未読み込みの場合は nullptr。</returns>
	static const EffectData* GetEffectData(const std::string& effectDataId);

	/// <summary>
	/// キャッシュ済み EffectData をすべて破棄する。
	/// </summary>
	static void UnloadAll();

private:
	static std::unordered_map<std::string, std::unique_ptr<EffectData>> resources;
};
