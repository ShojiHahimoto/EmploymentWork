#include "System/StateUpdateSystem.h"

#include "Component/CharacterAttackDataComponent.h"
#include "Component/CommandBufferComponent.h"
#include "Component/HitBoxComponent.h"
#include "Component/VelocityComponent.h"
#include "World/World.h"

#include <algorithm>

namespace
{
	constexpr int JumpStartupFrames = 4;
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
	CommandBufferComponent* commandBuffer = world.GetComponent<CommandBufferComponent>(objectId);
	HitBoxComponent* hitBox = world.GetComponent<HitBoxComponent>(objectId);
	CharacterAttackDataComponent* attackData = world.GetComponent<CharacterAttackDataComponent>(objectId);

	// 入力を持たない CPU やデバッグ対象でも、被弾や落下などの状態更新は必要なので入力履歴は任意にする。
	if (!state || !velocity)
	{
		return;
	}

	// このフレームをカウントしてから判定する。下で状態遷移した場合は ApplyActionState が 0 に戻す。
	++state->actionFrame;

	InputHistoryFrame neutralInputFrame;
	const bool hasInputHistory = inputHistory
		&& inputHistory->latestFrameIndex >= 0
		&& inputHistory->storedFrameCount > 0;
	const InputHistoryFrame& inputFrame = hasInputHistory
		? inputHistory->frames[inputHistory->latestFrameIndex]
		: neutralInputFrame;

	const PlayerActionDecision decision = DecideNextAction(*state, *velocity, inputFrame, commandBuffer);
	ApplyActionState(*state, hitBox, attackData, commandBuffer, decision);
}

/// <summary>
/// 被弾、キャンセル不可行動、通常入力の優先順で次に採用する行動を決める。
/// </summary>
/// <param name="state">現在の Player 状態。</param>
/// <param name="velocity">空中上昇・落下の判定に使う VelocityComponent。</param>
/// <param name="inputFrame">今フレームの入力履歴。</param>
/// <param name="commandBuffer">入力履歴から成立済みのコマンド候補。</param>
/// <returns>次の PlayerActionState と、同じ状態を最初からやり直すかどうか。</returns>
PlayerActionDecision StateUpdateSystem::DecideNextAction(
	const StateComponent& state,
	const VelocityComponent& velocity,
	const InputHistoryFrame& inputFrame,
	const CommandBufferComponent* commandBuffer)
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

	if (IsJumpStartupAction(state.currentActionState))
	{
		return DecideJumpStartupAction(state, inputFrame, commandBuffer);
	}

	if (IsLockedAction(state.currentActionState)
		&& !IsActionFinished(state)
		&& !CanCancelAction(state))
	{
		return { state.currentActionState, false };
	}

	return DecideNeutralAction(state, velocity, inputFrame, commandBuffer);
}

/// <summary>
/// CommandBufferComponent から攻撃候補を選び、地上/空中の攻撃 ActionState へ変換する。
/// </summary>
/// <param name="state">接地状態を持つ StateComponent。</param>
/// <param name="inputFrame">現在フレーム番号を持つ入力履歴。</param>
/// <param name="commandBuffer">攻撃入力から作られた先行入力候補。</param>
/// <returns>攻撃候補があれば攻撃 Decision。なければ Idle の空 Decision。</returns>
PlayerActionDecision StateUpdateSystem::DecideBufferedAttack(
	const StateComponent& state,
	const InputHistoryFrame& inputFrame,
	const CommandBufferComponent* commandBuffer)
{
	std::string bufferedAttackSlotId;
	int commandAcceptedFrame = -1;
	if (!TrySelectBufferedAttack(commandBuffer, state, inputFrame.frameNumber, bufferedAttackSlotId, commandAcceptedFrame))
	{
		return {};
	}

	return {
		state.isGrounded ? PlayerActionState::GroundAttack : PlayerActionState::AirAttack,
		true,
		bufferedAttackSlotId,
		true,
		commandAcceptedFrame
	};
}

/// <summary>
/// ジャンプ移行中の状態を処理し、攻撃入力があれば地上攻撃で上書きする。
/// </summary>
/// <param name="state">現在の JumpStartup 状態と actionFrame を持つ StateComponent。</param>
/// <param name="inputFrame">今フレームの入力履歴。</param>
/// <param name="commandBuffer">先行入力として保存された攻撃候補。</param>
/// <returns>攻撃、実ジャンプ、または JumpStartup 継続の Decision。</returns>
PlayerActionDecision StateUpdateSystem::DecideJumpStartupAction(
	const StateComponent& state,
	const InputHistoryFrame& inputFrame,
	const CommandBufferComponent* commandBuffer)
{
	PlayerActionDecision attackDecision = DecideBufferedAttack(state, inputFrame, commandBuffer);
	if (attackDecision.consumeCommand)
	{
		return attackDecision;
	}

	if (IsActionFinished(state))
	{
		return { ConvertJumpStartupToJump(state.currentActionState), true };
	}

	return { state.currentActionState, false };
}

/// <summary>
/// キャンセル不可などの制限がない時に、入力と接地状態から通常行動を選ぶ。
/// </summary>
/// <param name="state">接地状態などを確認する StateComponent。</param>
/// <param name="velocity">空中時の上昇・落下を確認する VelocityComponent。</param>
/// <param name="inputFrame">今フレームの入力履歴。</param>
/// <param name="commandBuffer">攻撃入力から作られた先行入力候補。</param>
/// <returns>通常状態から採用する PlayerActionState。</returns>
PlayerActionDecision StateUpdateSystem::DecideNeutralAction(
	const StateComponent& state,
	const VelocityComponent& velocity,
	const InputHistoryFrame& inputFrame,
	const CommandBufferComponent* commandBuffer)
{
	PlayerActionDecision attackDecision = DecideBufferedAttack(state, inputFrame, commandBuffer);
	if (attackDecision.consumeCommand)
	{
		return attackDecision;
	}

	if (state.isGrounded
		&& ((inputFrame.direction == 7 && state.facingDirection == FacingDirection::Right)
			|| (inputFrame.direction == 9 && state.facingDirection == FacingDirection::Left)))
	{
		return { PlayerActionState::BackJumpStartup, true };
	}
	if (state.isGrounded && inputFrame.direction == 8)
	{
		return { PlayerActionState::VerticalJumpStartup, true };
	}
	if (state.isGrounded
		&& ((inputFrame.direction == 9 && state.facingDirection == FacingDirection::Right)
			|| (inputFrame.direction == 7 && state.facingDirection == FacingDirection::Left)))
	{
		return { PlayerActionState::FrontJumpStartup, true };
	}

	if (!state.isGrounded)
	{
		return {
			velocity.velocity.y > 0.0f ? state.currentActionState : PlayerActionState::Fall,
			false
		};
	}

	if (HasHorizontalMoveDirection(inputFrame.direction))
	{
		if ((inputFrame.direction == 4 && state.facingDirection == FacingDirection::Left)
			|| (inputFrame.direction == 6 && state.facingDirection == FacingDirection::Right))
		{
			return { PlayerActionState::FrontWalk, false };
		}
		if ((inputFrame.direction == 4 && state.facingDirection == FacingDirection::Right)
			|| (inputFrame.direction == 6 && state.facingDirection == FacingDirection::Left))
		{
			return { PlayerActionState::BackWalk, false };
		}
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
/// 指定 ActionState がジャンプ移行フレーム中か判定する。
/// </summary>
/// <param name="actionState">判定する PlayerActionState。</param>
/// <returns>いずれかの JumpStartup 状態なら true。</returns>
bool StateUpdateSystem::IsJumpStartupAction(PlayerActionState actionState)
{
	return actionState == PlayerActionState::VerticalJumpStartup
		|| actionState == PlayerActionState::FrontJumpStartup
		|| actionState == PlayerActionState::BackJumpStartup;
}

/// <summary>
/// ジャンプ移行 ActionState を、対応する実ジャンプ ActionState に変換する。
/// </summary>
/// <param name="actionState">変換元の JumpStartup 状態。</param>
/// <returns>対応する Jump 状態。JumpStartup 以外の場合は Fall。</returns>
PlayerActionState StateUpdateSystem::ConvertJumpStartupToJump(PlayerActionState actionState)
{
	switch (actionState)
	{
	case PlayerActionState::VerticalJumpStartup:
		return PlayerActionState::VerticalJump;
	case PlayerActionState::FrontJumpStartup:
		return PlayerActionState::FrontJump;
	case PlayerActionState::BackJumpStartup:
		return PlayerActionState::BackJump;
	default:
		return PlayerActionState::Fall;
	}
}

/// <summary>
/// CommandBufferComponent 内の有効な攻撃候補から、新しいものを優先して 1 つ選ぶ。
/// </summary>
/// <param name="commandBuffer">検索対象の先行入力候補。</param>
/// <param name="state">発動条件の地上/空中判定に使う現在状態。</param>
/// <param name="currentFrameNumber">今フレームの入力履歴番号。</param>
/// <param name="outAttackSlotId">選ばれた攻撃スロット ID の書き込み先。</param>
/// <param name="outCommandAcceptedFrame">選ばれたコマンド成立フレームの書き込み先。</param>
/// <returns>実行できる攻撃候補があれば true。</returns>
bool StateUpdateSystem::TrySelectBufferedAttack(
	const CommandBufferComponent* commandBuffer,
	const StateComponent& state,
	int currentFrameNumber,
	std::string& outAttackSlotId,
	int& outCommandAcceptedFrame)
{
	if (!commandBuffer)
	{
		return false;
	}

	const BufferedCommandInput* bestCommand = nullptr;
	for (const BufferedCommandInput& command : commandBuffer->commands)
	{
		if (!command.valid
			|| command.attackSlotId.empty()
			|| command.bufferExpireFrame < currentFrameNumber
			|| !IsBufferedAttackUsableInCurrentState(command.usableState, state))
		{
			continue;
		}

		if (!bestCommand
			|| command.commandAcceptedFrame > bestCommand->commandAcceptedFrame
			|| (command.commandAcceptedFrame == bestCommand->commandAcceptedFrame
				&& command.priority > bestCommand->priority))
		{
			bestCommand = &command;
		}
	}

	if (!bestCommand)
	{
		return false;
	}

	outAttackSlotId = bestCommand->attackSlotId;
	outCommandAcceptedFrame = bestCommand->commandAcceptedFrame;
	return true;
}

/// <summary>
/// 先行入力候補が、実行する瞬間の地上/空中状態に合っているか確認する。
/// </summary>
/// <param name="usableState">攻撃データ側で指定された発動可能状態。</param>
/// <param name="state">現在の Player 状態。</param>
/// <returns>今の状態で発動できるなら true。</returns>
bool StateUpdateSystem::IsBufferedAttackUsableInCurrentState(AttackUsableState usableState, const StateComponent& state)
{
	switch (usableState)
	{
	case AttackUsableState::Ground:
		return state.isGrounded;
	case AttackUsableState::Air:
		return !state.isGrounded;
	case AttackUsableState::Both:
		return true;
	default:
		return false;
	}
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
	case PlayerActionState::VerticalJumpStartup:
	case PlayerActionState::FrontJumpStartup:
	case PlayerActionState::BackJumpStartup:
		return state.actionFrame >= JumpStartupFrames;
	case PlayerActionState::GroundAttack:
	case PlayerActionState::AirAttack:
		return state.actionDurationFrames <= 0 || state.actionFrame >= state.actionDurationFrames;
	case PlayerActionState::Hitstun:
		return state.actionFrame >= state.hitstunDurationFrames;
	default:
		return true;
	}
}

/// <summary>
/// 指定攻撃スロットから、攻撃全体の総フレーム数を計算する。
/// </summary>
/// <param name="attackData">キャラクターに割り当てられた技データ。</param>
/// <param name="attackSlotId">実行する攻撃 slotId。</param>
/// <returns>startup + active + recovery。取得できない場合は 0。</returns>
int StateUpdateSystem::CalculateAttackTotalFrames(
	const CharacterAttackDataComponent* attackData,
	const std::string& attackSlotId)
{
	if (!attackData || attackSlotId.empty())
	{
		return 0;
	}

	for (const CharacterAssignedAttackData& assignedAttack : attackData->attacks)
	{
		if (assignedAttack.slotId != attackSlotId)
		{
			continue;
		}

		const AttackFrameData& frame = assignedAttack.attack.frame;
		return std::max(0, frame.startup)
			+ std::max(0, frame.active)
			+ std::max(0, frame.recovery);
	}

	return 0;
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
/// <param name="hitBox">攻撃開始時に currentAttack を更新する HitBoxComponent。</param>
/// <param name="attackData">攻撃開始時に合計フレームを取得する CharacterAttackDataComponent。</param>
/// <param name="commandBuffer">採用済みコマンドを消費する CommandBufferComponent。</param>
/// <param name="decision">採用する ActionState と再開始フラグ。</param>
void StateUpdateSystem::ApplyActionState(
	StateComponent& state,
	HitBoxComponent* hitBox,
	const CharacterAttackDataComponent* attackData,
	CommandBufferComponent* commandBuffer,
	const PlayerActionDecision& decision)
{
	const bool actionChanged = state.currentActionState != decision.nextActionState || decision.restartAction;
	if (actionChanged)
	{
		const PlayerActionState previousActionState = state.currentActionState;
		const FacingDirection previousActionStartFacingDirection = state.actionStartFacingDirection;
		const bool isJumpStartupToJump = IsJumpStartupAction(previousActionState)
			&& ConvertJumpStartupToJump(previousActionState) == decision.nextActionState;

		state.currentActionState = decision.nextActionState;
		state.actionStartFacingDirection = isJumpStartupToJump
			? previousActionStartFacingDirection
			: state.facingDirection;
		state.actionFrame = 0;
		state.actionDurationFrames = 0;
		state.cancelEnabled = false;

		const bool isAttackState = state.currentActionState == PlayerActionState::GroundAttack
			|| state.currentActionState == PlayerActionState::AirAttack;
		if (isAttackState)
		{
			const std::string attackSlotId = decision.attackSlotId.empty() ? "AttackA" : decision.attackSlotId;
			state.actionDurationFrames = CalculateAttackTotalFrames(attackData, attackSlotId);

			if (hitBox)
			{
				hitBox->currentAttack.slotId = attackSlotId;
				hitBox->currentAttack.hasHit = false;
			}

			ConsumeBufferedCommand(commandBuffer, decision);
		}
		else if (hitBox)
		{
			hitBox->currentAttack.slotId.clear();
			hitBox->currentAttack.hasHit = false;
		}
	}

	state.hitstunRequested = false;
}

/// <summary>
/// 採用したコマンド候補を CommandBufferComponent から消す。
/// </summary>
/// <param name="commandBuffer">消費対象の CommandBufferComponent。</param>
/// <param name="decision">採用した攻撃スロットと成立フレームを持つ決定情報。</param>
void StateUpdateSystem::ConsumeBufferedCommand(
	CommandBufferComponent* commandBuffer,
	const PlayerActionDecision& decision)
{
	if (!commandBuffer || !decision.consumeCommand)
	{
		return;
	}

	for (BufferedCommandInput& command : commandBuffer->commands)
	{
		if (command.valid
			&& command.attackSlotId == decision.attackSlotId
			&& command.commandAcceptedFrame == decision.commandAcceptedFrame)
		{
			command = BufferedCommandInput{};
			return;
		}
	}
}
