#include "System/StateUpdateSystem.h"

#include "System/Debugger.h"
#include "Component/TransformComponent.h"
#include "Component/VelocityComponent.h"
#include "World/World.h"

namespace
{
	constexpr int GroundAttackDurationFrames = 24;
	constexpr int AirAttackDurationFrames = 24;
	constexpr int HitstunDurationFrames = 30;
}

void StateUpdateSystem::Update(World& world)
{
	for (GameObject& object : world.GetGameObjects())
	{
		if (object.tag != GameObjectTag::Player)
		{
			continue;
		}

		UpdatePlayerState(world, object.id);
	}
}

/// <summary>
/// 1体の Player について、入力履歴と現在状態から今フレームの PlayerActionState を確定する。
/// </summary>
/// <param name="world">対象 Component を取得する World。</param>
/// <param name="objectId">状態を更新する Player GameObject の ID。</param>
void StateUpdateSystem::UpdatePlayerState(World& world, GameObjectId objectId)
{
	StateComponent* state = world.GetComponent<StateComponent>(objectId);
	VelocityComponent* velocity = world.GetComponent<VelocityComponent>(objectId);
	InputHistoryComponent* inputHistory = world.GetComponent<InputHistoryComponent>(objectId);
	if (!world.GetTransform(objectId) || !state || !velocity || !inputHistory)
	{
		return;
	}

	// Count this frame first. If the action changes below, ApplyActionState resets it to 0.
	++state->actionFrame;

	const PlayerActionDecision decision = DecideNextAction(*state, *velocity, *inputHistory);
	ApplyActionState(*state, decision);
}

/// <summary>
/// 被弾、キャンセル不可行動、通常入力の優先順で次に採用する行動を決める。
/// </summary>
/// <param name="state">現在の Player 状態。</param>
/// <param name="velocity">空中上昇・落下の判定に使う VelocityComponent。</param>
/// <param name="inputHistory">今フレームの入力履歴。</param>
/// <returns>次の PlayerActionState と、同じ状態を最初からやり直すかどうか。</returns>
PlayerActionDecision StateUpdateSystem::DecideNextAction(
	const StateComponent& state,
	const VelocityComponent& velocity,
	const InputHistoryComponent& inputHistory)
{
	const InputHistoryFrame& inputFrame = inputHistory.frames[inputHistory.latestFrameIndex];

	if (state.hitstunRequested)
	{
		return { PlayerActionState::Hitstun, true };
	}

	if (IsLockedAction(state.currentActionState)
		&& !IsActionFinished(state)
		&& !CanCancelAction(state))
	{
		return { state.currentActionState, false };
	}

	return DecideNeutralAction(state, velocity, inputFrame);

	
}

/// <summary>
/// キャンセル不可などの制限がない時に、入力と接地状態から通常行動を選ぶ。
/// </summary>
/// <param name="state">接地状態などを確認する StateComponent。</param>
/// <param name="velocity">空中時の上昇・落下を確認する VelocityComponent。</param>
/// <param name="inputFrame">今フレームの入力履歴。</param>
/// <returns>通常状態から採用する PlayerActionState。</returns>
PlayerActionDecision StateUpdateSystem::DecideNeutralAction(
	const StateComponent& state,
	const VelocityComponent& velocity,
	const InputHistoryFrame& inputFrame)
{
	if (HasAttackTrigger(inputFrame))
	{
		return {
			state.isGrounded ? PlayerActionState::GroundAttack : PlayerActionState::AirAttack,
			true
		};
	}

	// 後からプレイヤーの向きに応じて前ジャンプと後ろジャンプを区別できるようにする
	if (state.isGrounded && inputFrame.direction == 7)
	{
		return { PlayerActionState::BackJump, true };
	}
	if (state.isGrounded && inputFrame.direction == 8)
	{
		return { PlayerActionState::VerticalJump, true };
	}
	if (state.isGrounded && inputFrame.direction == 9)
	{
		return { PlayerActionState::FrontJump, true };
	}

	if (!state.isGrounded)
	{
		return {
			velocity.velocity.y > 0.0f ? /*PlayerActionState::VerticalJump*/ state.currentActionState : PlayerActionState::Fall,
			false
		};
	}

	return {
		HasHorizontalMoveDirection(inputFrame.direction) ? PlayerActionState::Walk : PlayerActionState::Idle,
		false
	};
}

/// <summary>
/// テンキー方向が横移動を含むか判定する。
/// </summary>
/// <param name="direction">判定するテンキー方向。</param>
/// <returns>左または右入力を含んでいれば true。</returns>
bool StateUpdateSystem::HasHorizontalMoveDirection(int direction)
{
	return direction == 1 || direction == 3
		|| direction == 4 || direction == 6
		|| direction == 7 || direction == 9;
}

/// <summary>
/// 今フレームに攻撃ボタンの Trigger があるか判定する。
/// </summary>
/// <param name="inputFrame">判定する入力履歴。</param>
/// <returns>いずれかの攻撃ボタンが Trigger なら true。</returns>
bool StateUpdateSystem::HasAttackTrigger(const InputHistoryFrame& inputFrame)
{
	return inputFrame.lightAttack.trigger
		|| inputFrame.mediumAttack.trigger
		|| inputFrame.heavyAttack.trigger;
}

/// <summary>
/// 指定 ActionState が、終了またはキャンセルまで他行動へ移れない状態か判定する。
/// </summary>
/// <param name="actionState">判定する PlayerActionState。</param>
/// <returns>キャンセル不可管理が必要な状態なら true。</returns>
bool StateUpdateSystem::IsLockedAction(PlayerActionState actionState)
{
	return actionState == PlayerActionState::GroundAttack
		|| actionState == PlayerActionState::AirAttack
		|| actionState == PlayerActionState::Hitstun;
}

/// <summary>
/// 現在の ActionState が持続時間を終えているか確認する。
/// </summary>
/// <param name="state">ActionState と actionFrame を持つ StateComponent。</param>
/// <returns>行動が終了していれば true。</returns>
bool StateUpdateSystem::IsActionFinished(const StateComponent& state)
{
	switch (state.currentActionState)
	{
	case PlayerActionState::GroundAttack:
		return state.actionFrame >= GroundAttackDurationFrames;
	case PlayerActionState::AirAttack:
		return state.actionFrame >= AirAttackDurationFrames;
	case PlayerActionState::Hitstun:
		return state.actionFrame >= HitstunDurationFrames;
	default:
		return true;
	}
}

/// <summary>
/// 現在の行動をキャンセルして別行動へ移れるか確認する。
/// </summary>
/// <param name="state">キャンセル可否を持つ StateComponent。</param>
/// <returns>キャンセル可能なら true。</returns>
bool StateUpdateSystem::CanCancelAction(const StateComponent& state)
{
	return state.cancelEnabled;
}

/// <summary>
/// 決定した ActionState を StateComponent に反映し、必要なら actionFrame を 0 に戻す。
/// </summary>
/// <param name="state">更新する StateComponent。</param>
/// <param name="decision">採用する ActionState と再開始フラグ。</param>
void StateUpdateSystem::ApplyActionState(StateComponent& state, const PlayerActionDecision& decision)
{
	if (state.currentActionState != decision.nextActionState || decision.restartAction)
	{
		state.currentActionState = decision.nextActionState;
		state.actionFrame = 0;
		state.cancelEnabled = false;
	}

	state.hitstunRequested = false;
}
