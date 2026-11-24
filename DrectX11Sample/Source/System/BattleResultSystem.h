#pragma once

class World;

class BattleResultSystem
{
public:
	static void Update(World& world);

private:
	static bool ResolveKnockOut(World& world);
	static bool UpdateTimerAndResolveTimeUp(World& world);
};
