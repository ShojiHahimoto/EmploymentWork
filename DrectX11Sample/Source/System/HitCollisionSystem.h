#pragma once

class World;

class HitCollisionSystem
{
public:
	/// <summary>
	/// 攻撃側の AttackBox と防御側の HurtBox の接触を収集する。
	/// </summary>
	/// <param name="world">判定対象の GameObject と Component を保持する World。</param>
	static void Update(World& world);
};
