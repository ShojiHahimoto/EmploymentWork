#pragma once

class World;

class BattleResultSystem
{
public:
	/// <summary>
	/// HP とラウンドタイマーを確認し、勝敗が決まった場合は World に BattleResult を記録する。
	/// </summary>
	/// <param name="world">BattlePlayerId、HealthComponent、BattleTimerComponent を保持する World。</param>
	static void Update(World& world);

private:
	static bool ResolveKnockOut(World& world);
	static bool UpdateTimerAndResolveTimeUp(World& world);
};
