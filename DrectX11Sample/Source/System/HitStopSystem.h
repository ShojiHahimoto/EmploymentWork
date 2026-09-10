#pragma once

#include "World/World.h"

struct HitCollisionResult;

struct HitStopFrameSettings
{
	int guardFrames = 3;
	int normalAttackFrames = 6;
	int specialAttackFrames = 9;
	int clashFrames = 11;
};

class HitStopSystem
{
public:
	static void Update(World& world);

	/// <summary>
	/// 解決済みのヒット/ガード結果から、対応するヒットストップを World へ要求する。
	/// </summary>
	/// <param name="world">ヒットストップ状態を保持する World。</param>
	/// <param name="result">HitCollisionSystem が収集した攻撃接触結果。</param>
	/// <param name="guarded">ガードとして解決された場合は true。</param>
	/// <param name="mutualHit">同一フレームで相打ちになっている場合は true。</param>
	static void RequestFromResolvedHit(
		World& world,
		const HitCollisionResult& result,
		bool guarded,
		bool mutualHit);

	/// <summary>
	/// ヒットストップ段階ごとの停止フレーム数を取得する。
	/// </summary>
	/// <param name="level">確認するヒットストップ段階。</param>
	/// <returns>停止フレーム数。None の場合は 0。</returns>
	static int GetHitStopFrames(HitStopLevel level);

	/// <summary>
	/// 現在使用しているヒットストップ共通設定を取得する。
	/// </summary>
	/// <returns>ガード、通常技、必殺技、相打ちごとの停止フレーム設定。</returns>
	static const HitStopFrameSettings& GetFrameSettings();

private:
	static HitStopLevel DecideHitStopLevel(const HitCollisionResult& result, bool guarded, bool mutualHit);
};
