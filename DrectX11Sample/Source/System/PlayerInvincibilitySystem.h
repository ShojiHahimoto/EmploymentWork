#pragma once

struct StateComponent;
class World;

class PlayerInvincibilitySystem
{
public:
	/// <summary>
	/// Player の現在状態から、攻撃を受けないかどうかを StateComponent に反映する。
	/// </summary>
	/// <param name="world">Player GameObject と StateComponent を保持する World。</param>
	static void Update(World& world);

	/// <summary>
	/// ActionState と接地状態だけで決まる無敵かどうかを判定する。
	/// </summary>
	/// <param name="state">判定対象の StateComponent。</param>
	/// <returns>状態由来で攻撃を受けないなら true。</returns>
	static bool IsInvincibleByState(const StateComponent& state);
};
