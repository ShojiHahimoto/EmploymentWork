#pragma once

#include "Component/InputHistoryComponent.h"
#include "Component/StateComponent.h"
#include "Component/VelocityComponent.h"
#include "Core/GameObject.h"

class World;

struct PlayerControlFrameResult
{
	// PlayerActionState に応じた今フレームの挙動結果。
	// 現段階では歩き / 停止用の横速度だけを扱う。
	float horizontalVelocity = 0.0f;
};

class PlayerControlSystem
{
public:
	// StateUpdateSystem が確定した PlayerActionState に応じて、行動ごとの処理を行う。
	static void Update(World& world);

private:
	// 固定 60fps 前提の 1 フレームあたり横移動量。
	static constexpr float MoveSpeedPerFrame = 0.08f;

	static void UpdatePlayer(World& world, GameObjectId objectId);
	static PlayerControlFrameResult ExecuteCurrentAction(const StateComponent& state, const InputHistoryComponent& inputHistory);
	static float GetHorizontalInputFromDirection(int direction);
	static void ApplyFrameResult(VelocityComponent& velocity, const PlayerControlFrameResult& result);
};
