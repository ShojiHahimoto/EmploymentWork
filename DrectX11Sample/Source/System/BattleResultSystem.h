#pragma once

#include <array>

class World;
enum class BattleResult;

enum class RoundEndReason
{
	None,
	KnockOut,
	DoubleKnockOut,
	TimeOver
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
	static void BeginRound(World& world, BattleRoundState& round);
	static void AdvanceWait(World& world, BattleRoundState& round);
	static void Update(World& world, BattleRoundState& round);
	static BattleResult GetMatchResult(const BattleRoundState& round);

private:
	static bool ResolveKnockOut(World& world);
	static bool UpdateTimerAndResolveTimeUp(World& world);
};
