#pragma once

#include "Core/World.h"

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

	static void RequestFromResolvedHit(
		World& world,
		const HitCollisionResult& result,
		bool guarded,
		bool mutualHit);

	static int GetHitStopFrames(HitStopLevel level);

	static const HitStopFrameSettings& GetFrameSettings();

private:
	static HitStopLevel DecideHitStopLevel(const HitCollisionResult& result, bool guarded, bool mutualHit);
};
