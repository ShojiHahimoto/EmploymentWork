#pragma once

#include "Component/InputHistoryComponent.h"
#include "Component/StateComponent.h"
#include "Component/VelocityComponent.h"
#include "Core/GameObject.h"

class World;

struct PlayerControlFrameResult
{
	float horizontalVelocity = 0.0f;
	float verticalVelocity = 0.0f;
	bool setVerticalVelocity = false;
};

class PlayerControlSystem
{
public:
	// StateUpdateSystem が確定した PlayerActionState に応じて、行動ごとの処理を行う。
	static void Update(World& world);

private:
	static constexpr float MoveSpeedPerFrame = 0.08f;
	static constexpr float JumpInitialVelocity = 0.32f;

	static void UpdatePlayer(World& world, GameObjectId objectId);
	static PlayerControlFrameResult ExecuteCurrentAction(const StateComponent& state, const InputHistoryComponent& inputHistory);
	static float GetHorizontalInputFromDirection(int direction);
	static void ApplyFrameResult(VelocityComponent& velocity, const PlayerControlFrameResult& result);
};
