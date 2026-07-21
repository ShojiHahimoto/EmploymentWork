#pragma once

#include "Component/InputHistoryComponent.h"
#include "Component/StateComponent.h"
#include "Component/VelocityComponent.h"
#include "Core/GameObject.h"

#include <SimpleMath.h>

class World;

struct PlayerControlFrameResult
{
	float horizontalInput = 0.0f;
	ObjectState nextState = ObjectState::Idle;
	DirectX::SimpleMath::Vector3 velocity = DirectX::SimpleMath::Vector3::Zero;
};

class PlayerControlSystem
{
public:
	// Player タグと必要 Component を持つ GameObject に、保存済み入力履歴を反映する。
	static void Update(World& world);

private:
	// 固定 60fps 前提の 1 フレームあたり横移動量。
	static constexpr float MoveSpeedPerFrame = 0.08f;

	static void UpdatePlayer(World& world, GameObjectId objectId);
	static PlayerControlFrameResult DecideFrameResult(const StateComponent& state, const InputHistoryComponent& inputHistory);
	static ObjectState DecideGroundState(float horizontalInput);
	static void ApplyFrameResult(VelocityComponent& velocity, StateComponent& state, const PlayerControlFrameResult& result);
};
