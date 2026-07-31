#pragma once

#include "Core/GameObject.h"

class World;

class HitResolveSystem
{
public:
	/// <summary>
	/// HitCollisionSystem が収集したヒット結果を、State や HitBox の結果へ確定する。
	/// </summary>
	/// <param name="world">ヒット結果と Component を保持する World。</param>
	static void Update(World& world);

private:
	static void MarkAttackAsHit(World& world, GameObjectId attackerId);
	static void ApplyDamage(World& world, GameObjectId defenderId, int damage);
	static void ApplyHitstun(World& world, GameObjectId defenderId, int hitstunFrames);
	static void ResolveBattleResult(World& world);
};
