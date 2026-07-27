#pragma once

#include "Component/InputHistoryComponent.h"
#include "Component/StateComponent.h"
#include "Component/VelocityComponent.h"
#include "Data/CharacterData.h"
#include "Core/GameObject.h"

class World;
struct TransformComponent;

struct PlayerControlFrameResult
{
	float horizontalVelocity = 0.0f;
	bool setHorizontalVelocity = true;
	float verticalVelocity = 0.0f;
	bool setVerticalVelocity = false;
};

class PlayerControlSystem
{
public:
	// StateUpdateSystem が確定した PlayerActionState に応じて、行動ごとの処理を行う。
	static void Update(World& world);

private:
	static void UpdatePlayer(World& world, GameObjectId objectId);
	static PlayerControlFrameResult ExecuteCurrentAction(const StateComponent& state, const InputHistoryComponent& inputHistory, const CharacterParameterData& parameter);
	static float GetHorizontalInputFromDirection(int direction);
	static void ApplyFrameResult(VelocityComponent& velocity, const PlayerControlFrameResult& result);
	static void ApplyPlayerDirection(const StateComponent& state, TransformComponent& transform);
};
