#include "System/StateUpdateSystem.h"

#include "System/Debugger.h"
#include "System/TransformSystem.h"
#include "Component/HitBoxComponent.h"
#include "Component/TransformComponent.h"
#include "Component/VelocityComponent.h"
#include "World/World.h"

namespace
{
	constexpr int GroundAttackDurationFrames = 24;
	constexpr int AirAttackDurationFrames = 24;
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
	// 必要コンポーネント取得
	StateComponent* state = world.GetComponent<StateComponent>(objectId);
	VelocityComponent* velocity = world.GetComponent<VelocityComponent>(objectId);
	InputHistoryComponent* inputHistory = world.GetComponent<InputHistoryComponent>(objectId);
	TransformComponent* transform = world.GetComponent<TransformComponent>(objectId);
	HitBoxComponent* hitBox = world.GetComponent<HitBoxComponent>(objectId);

	// 入力を持たないデバッグ用 2P や CPU も、被弾や落下などの状態更新は必要なので入力履歴は任意にする。
	if (!state || !velocity || !transform)
	{
		return;
	}

	// Count this frame first. If the action changes below, ApplyActionState resets it to 0.
	++state->actionFrame;

	ApplyPlayerDirection(world, objectId, *state, *transform);

	InputHistoryFrame neutralInputFrame;
	const InputHistoryFrame& inputFrame = inputHistory
		? inputHistory->frames[inputHistory->latestFrameIndex]
		: neutralInputFrame;

	const PlayerActionDecision decision = DecideNextAction(*state, *velocity, inputFrame);
	ApplyActionState(*state, hitBox, decision);
}

/// <summary>
/// 被弾、キャンセル不可行動、通常入力の優先順で次に採用する行動を決める。
/// </summary>
/// <param name="state">現在の Player 状態。</param>
/// <param name="velocity">空中上昇・落下の判定に使う VelocityComponent。</param>
/// <param name="inputFrame">今フレームの入力履歴。</param>
/// <returns>次の PlayerActionState と、同じ状態を最初からやり直すかどうか。</returns>
PlayerActionDecision StateUpdateSystem::DecideNextAction(
	const StateComponent& state,
	const VelocityComponent& velocity,
	const InputHistoryFrame& inputFrame)
{
	if (state.hitstunRequested)
	{
		return { PlayerActionState::Hitstun, true };
	}

	if (state.currentActionState == PlayerActionState::AirAttack)
	{
		if (state.isGrounded)
		{
			return { PlayerActionState::Idle, false };
		}
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
			true,
			SelectAttackSlotId(inputFrame)
		};
	}

	// 後からプレイヤーの向きに応じて前ジャンプと後ろジャンプを区別できるようにする
	if (state.isGrounded && (inputFrame.direction == 7 && state.facingDirection == FacingDirection::Right
						  || inputFrame.direction == 9 && state.facingDirection == FacingDirection::Left))
	{
		return { PlayerActionState::BackJump, true };
	}
	if (state.isGrounded && inputFrame.direction == 8)
	{
		return { PlayerActionState::VerticalJump, true };
	}
	if (state.isGrounded && (inputFrame.direction == 9 && state.facingDirection == FacingDirection::Right
						  || inputFrame.direction == 7 && state.facingDirection == FacingDirection::Left))
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

	//return {
	//	HasHorizontalMoveDirection(inputFrame.direction) ? PlayerActionState::FrontWalk : PlayerActionState::Idle,
	//	false
	//};

	if (HasHorizontalMoveDirection(inputFrame.direction))
	{
		if (inputFrame.direction == 4 && state.facingDirection == FacingDirection::Left ||
			inputFrame.direction == 6 && state.facingDirection == FacingDirection::Right)
		{
			return { PlayerActionState::FrontWalk, false };
		}
		else if (inputFrame.direction == 4 && state.facingDirection == FacingDirection::Right ||
			inputFrame.direction == 6 && state.facingDirection == FacingDirection::Left)
		{
			return { PlayerActionState::BackWalk, false };
		}
	}
	else
	{
		return { PlayerActionState::Idle, false };
	}

	return { PlayerActionState::Idle, false };
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
/// 攻撃入力から実行する技スロット ID を選ぶ。
/// </summary>
/// <param name="inputFrame">判定する入力履歴。</param>
/// <returns>入力に対応する slotId。未入力の場合は空文字。</returns>
std::string StateUpdateSystem::SelectAttackSlotId(const InputHistoryFrame& inputFrame)
{
	if (inputFrame.lightAttack.trigger)
	{
		return "Attack1";
	}

	if (inputFrame.mediumAttack.trigger)
	{
		return "Attack2";
	}

	if (inputFrame.heavyAttack.trigger)
	{
		return "Attack3";
	}

	return "";
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
		return state.actionFrame >= state.hitstunDurationFrames;
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
void StateUpdateSystem::ApplyActionState(StateComponent& state, HitBoxComponent* hitBox, const PlayerActionDecision& decision)
{
	if (state.currentActionState != decision.nextActionState || decision.restartAction)
	{
		state.currentActionState = decision.nextActionState;
		state.actionFrame = 0;
		state.cancelEnabled = false;

		if (hitBox)
		{
			if (state.currentActionState == PlayerActionState::GroundAttack
				|| state.currentActionState == PlayerActionState::AirAttack)
			{
				hitBox->currentAttack.slotId = decision.attackSlotId.empty() ? "Attack1" : decision.attackSlotId;
				hitBox->currentAttack.hasHit = false;
			}
			else
			{
				hitBox->currentAttack.slotId.clear();
				hitBox->currentAttack.hasHit = false;
			}
		}
	}

	state.hitstunRequested = false;
}

/// <summary>
/// 相手 Player との X 座標関係から、ステートコンポーネント内のプレイヤー向きを更新する。
/// </summary>
/// <param name="world">相手 Player の Transform を取得する World。</param>
/// <param name="objectId">向きを更新する Player GameObject ID。</param>
/// <param name="state">向き情報を書き込む StateComponent。</param>
/// <param name="transform">自分の X 座標を確認する TransformComponent。</param>
void StateUpdateSystem::ApplyPlayerDirection(
	World& world,
	GameObjectId objectId,
	StateComponent& state,
	const TransformComponent& transform)
{
	if (state.currentActionState == PlayerActionState::Idle
		|| state.currentActionState == PlayerActionState::FrontWalk
		|| state.currentActionState == PlayerActionState::BackWalk)
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
}
