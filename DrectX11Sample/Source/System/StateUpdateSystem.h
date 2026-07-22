#pragma once

#include "Component/InputHistoryComponent.h"
#include "Component/StateComponent.h"
#include "Core/GameObject.h"

class World;

class StateUpdateSystem
{
public:
	// 入力履歴と現在状態を見て、今フレームの PlayerActionState と actionFrame を更新する。
	static void Update(World& world);

private:
	static void UpdatePlayerState(World& world, GameObjectId objectId);
	static PlayerActionState DecideNextActionState(const StateComponent& state, const InputHistoryComponent& inputHistory);
	static PlayerActionState DecideNeutralActionState(const StateComponent& state, const InputHistoryFrame& inputFrame);
	static bool HasHorizontalMoveDirection(int direction);
	static bool HasAttackTrigger(const InputHistoryFrame& inputFrame);
	static bool IsLockedAction(PlayerActionState actionState);
	static bool IsActionFinished(const StateComponent& state);
	static bool CanCancelAction(const StateComponent& state);
	static void ApplyActionState(StateComponent& state, PlayerActionState nextActionState);
};
