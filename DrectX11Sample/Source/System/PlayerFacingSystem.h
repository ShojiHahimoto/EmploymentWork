#pragma once

#include "Component/StateComponent.h"
#include "Core/GameObject.h"

class World;
struct TransformComponent;

class PlayerFacingSystem
{
public:
	// 相手 Player との位置関係から、対面方向を更新する。
	static void Update(World& world);

private:
	static void UpdatePlayerFacing(World& world, GameObjectId objectId);
	static bool ShouldUpdateFacing(const StateComponent& state);
	static void ApplyFacingFromOpponent(
		World& world,
		GameObjectId objectId,
		StateComponent& state,
		const TransformComponent& transform);
};
