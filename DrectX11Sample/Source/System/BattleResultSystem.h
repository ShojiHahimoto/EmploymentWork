#pragma once

#include <array>

class World;
enum class BattleResult;

/// <summary>
/// ラウンド決着の理由
/// </summary>
enum class RoundEndReason
{
	None,
	KnockOut,
	DoubleKnockOut,
	TimeOver
};

/// <summary>60fpsの固定フレーム単位で指定するラウンド演出時間。</summary>
struct BattleRoundTiming
{
	int roundIntroDelayFrames = 24;
	int roundDisplayFrames = 60;
	int roundToFightFrames = 10;
	int fightDisplayFrames = 32;
	int fightToControlFrames = 4;
	int roundEndDisplayDelayFrames = 5;
	int roundEndDisplayFrames = 56;
	int roundEndAfterDisplayFrames = 8;
	int releaseToVictoryFrames = 30;
	int victoryMotionFrames = 115;
	int matchResultDisplayFrames = 85;
	int doubleKnockoutDownFrames = 60;
	int fadeOutFrames = 20;
	int fadeInFrames = 18;
};

struct BattleRoundState
{
	std::array<int, 2> points{};
	int number = 1;
	int waitFrames = 0;
	RoundEndReason endReason = RoundEndReason::None;
};

class BattleResultSystem
{
public:
	static BattleRoundTiming LoadTiming();
	static void BeginRound(World& world, BattleRoundState& round, int introFrames);
	static void AdvanceWait(World& world, BattleRoundState& round);
	static void Update(World& world, BattleRoundState& round);
	static BattleResult GetMatchResult(const BattleRoundState& round);

private:
	static bool ResolveKnockOut(World& world);
	static bool UpdateTimerAndResolveTimeUp(World& world);
};
