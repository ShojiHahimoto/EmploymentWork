#include "System/HitStopSystem.h"

#include "Data/AttackData.h"
#include "World/World.h"

namespace
{
	constexpr HitStopFrameSettings DefaultFrameSettings
	{
		3,
		6,
		9,
		11
	};
}

void HitStopSystem::Update(World& world)
{
	world.AdvanceHitStopFrame();
}

/// <summary>
/// 解決済みのヒット/ガード結果から、対応するヒットストップを World へ要求する。
/// </summary>
/// <param name="world">ヒットストップ状態を保持する World。</param>
/// <param name="result">HitCollisionSystem が収集した攻撃接触結果。</param>
/// <param name="guarded">ガードとして解決された場合は true。</param>
/// <param name="mutualHit">同一フレームで相打ちになっている場合は true。</param>
void HitStopSystem::RequestFromResolvedHit(
	World& world,
	const HitCollisionResult& result,
	bool guarded,
	bool mutualHit)
{
	const HitStopLevel level = DecideHitStopLevel(result, guarded, mutualHit);
	world.RequestHitStop(level, GetHitStopFrames(level));
}

/// <summary>
/// ヒットストップ段階ごとの停止フレーム数を取得する。
/// </summary>
/// <param name="level">確認するヒットストップ段階。</param>
/// <returns>停止フレーム数。None の場合は 0。</returns>
int HitStopSystem::GetHitStopFrames(HitStopLevel level)
{
	const HitStopFrameSettings& settings = GetFrameSettings();

	switch (level)
	{
	case HitStopLevel::Guard:
		return settings.guardFrames;
	case HitStopLevel::NormalAttack:
		return settings.normalAttackFrames;
	case HitStopLevel::SpecialAttack:
		return settings.specialAttackFrames;
	case HitStopLevel::Clash:
		return settings.clashFrames;
	case HitStopLevel::None:
	default:
		return 0;
	}
}

/// <summary>
/// 現在使用しているヒットストップ共通設定を取得する。
/// </summary>
/// <returns>ガード、通常技、必殺技、相打ちごとの停止フレーム設定。</returns>
const HitStopFrameSettings& HitStopSystem::GetFrameSettings()
{
	return DefaultFrameSettings;
}

/// <summary>
/// ヒット結果の状況から、今回使うヒットストップ段階を決める。
/// </summary>
/// <param name="result">攻撃種別を含むヒット結果。</param>
/// <param name="guarded">ガード成立なら true。</param>
/// <param name="mutualHit">相打ち成立なら true。</param>
/// <returns>適用するヒットストップ段階。</returns>
HitStopLevel HitStopSystem::DecideHitStopLevel(
	const HitCollisionResult& result,
	bool guarded,
	bool mutualHit)
{
	if (mutualHit)
	{
		return HitStopLevel::Clash;
	}

	if (guarded)
	{
		return HitStopLevel::Guard;
	}

	if (result.attackKind == AttackKind::Special)
	{
		return HitStopLevel::SpecialAttack;
	}

	return HitStopLevel::NormalAttack;
}
