#include "System/PlayerFacingSystem.h"

#include "Component/TransformComponent.h"
#include "System/TransformSystem.h"
#include "World/World.h"

void PlayerFacingSystem::Update(World& world)
{
	for (int playerIndex = 0; playerIndex < World::BattlePlayerCount; ++playerIndex)
	{
		const GameObjectId objectId = world.GetBattlePlayerId(playerIndex);
		if (objectId == INVALID_GAME_OBJECT_ID)
		{
			continue;
		}

		UpdatePlayerFacing(world, objectId);
	}
}

/// <summary>
/// 1体の Player について、向き更新が許可される状態なら相手側を向かせる。
/// </summary>
/// <param name="world">相手 Player と Transform を取得する World。</param>
/// <param name="objectId">向きを更新する Player GameObject ID。</param>
void PlayerFacingSystem::UpdatePlayerFacing(World& world, GameObjectId objectId)
{
	StateComponent* state = world.GetComponent<StateComponent>(objectId);
	const TransformComponent* transform = world.GetTransform(objectId);
	if (!state || !transform || !ShouldUpdateFacing(*state))
	{
		return;
	}

	ApplyFacingFromOpponent(world, objectId, *state, *transform);
}

/// <summary>
/// 現在の ActionState が、相手との位置で振り向いてよい状態か判定する。
/// </summary>
/// <param name="state">確認する StateComponent。</param>
/// <returns>接地中かつ振り向き可能な状態なら true。</returns>
bool PlayerFacingSystem::ShouldUpdateFacing(const StateComponent& state)
{
	if (!state.isGrounded)
	{
		return false;
	}

	if (state.currentActionState == PlayerActionState::Idle
		|| state.currentActionState == PlayerActionState::FrontWalk
		|| state.currentActionState == PlayerActionState::BackWalk
		|| state.currentActionState == PlayerActionState::VerticalJumpStartup
		|| state.currentActionState == PlayerActionState::FrontJumpStartup
		|| state.currentActionState == PlayerActionState::BackJumpStartup
		|| state.currentActionState == PlayerActionState::VerticalJump
		|| state.currentActionState == PlayerActionState::FrontJump
		|| state.currentActionState == PlayerActionState::BackJump
		|| state.currentActionState == PlayerActionState::Fall)
	{
		return true;
	}
	return false;
}

/// <summary>
/// 相手 Player との X 座標関係から、ステートコンポーネント内のプレイヤー向きを更新する。
/// </summary>
/// <param name="world">相手 Player の Transform を取得する World。</param>
/// <param name="objectId">向きを更新する Player GameObject ID。</param>
/// <param name="state">向き情報を書き込む StateComponent。</param>
/// <param name="transform">自分の X 座標を確認する TransformComponent。</param>
void PlayerFacingSystem::ApplyFacingFromOpponent(
	World& world,
	GameObjectId objectId,
	StateComponent& state,
	const TransformComponent& transform)
{
	const GameObjectId opponentId = world.GetOpponentBattlePlayerId(objectId);
	const TransformComponent* opponentTransform = world.GetTransform(opponentId);
	if (!opponentTransform)
	{
		return;
	}

	const float selfX = TransformSystem::GetLocalPosition(transform).x;
	const float opponentX = TransformSystem::GetLocalPosition(*opponentTransform).x;
	if (selfX < opponentX)
	{
		state.facingDirection = FacingDirection::Right;
	}
	else if (selfX > opponentX)
	{
		state.facingDirection = FacingDirection::Left;
	}
}
