#include "System/CommandInputSystem.h"

#include "Component/CommandBufferComponent.h"
#include "World/World.h"

namespace
{
	constexpr int NormalAttackYPriority = 101;
	constexpr int NormalAttackXPriority = 102;
	constexpr int NormalAttackBPriority = 103;
	constexpr int NormalAttackAPriority = 104;
	constexpr int HadokenPriority = 200;
	constexpr int ShoryuPriority = 300;

	constexpr int DirectionUp = 1 << 0;
	constexpr int DirectionDown = 1 << 1;
	constexpr int DirectionBack = 1 << 2;
	constexpr int DirectionForward = 1 << 3;

	constexpr CommandDirectionStep HadokenCommand[] =
	{
		{ 2, CommandDirectionMatchMode::ContainsComponents },
		{ 3, CommandDirectionMatchMode::ContainsComponents },
		{ 6, CommandDirectionMatchMode::ContainsComponents },
	};

	// 簡易昇竜は「前成分 -> 下成分 -> 前成分」を見る。
	// 623 だけでなく 636 や 323 なども成立し、同じ方向の押しっぱなしだけでは成立しない。
	constexpr CommandDirectionStep ShoryuCommand[] =
	{
		{ 6, CommandDirectionMatchMode::ContainsComponents },
		{ 2, CommandDirectionMatchMode::ContainsComponents },
		{ 6, CommandDirectionMatchMode::ContainsComponents },
	};

	/// <summary>
	/// 指定 mask に任意の入力が含まれているか確認する。
	/// </summary>
	/// <param name="mask">判定対象の入力 mask。</param>
	/// <param name="buttonMask">確認するボタン mask。</param>
	/// <returns>buttonMask が含まれていれば true。</returns>
	bool HasButton(uint32_t mask, uint32_t buttonMask)
	{
		return (mask & buttonMask) != 0;
	}
}

void CommandInputSystem::Update(World& world)
{
	for (int playerIndex = 0; playerIndex < World::BattlePlayerCount; ++playerIndex)
	{
		const GameObjectId objectId = world.GetBattlePlayerId(playerIndex);
		if (objectId == INVALID_GAME_OBJECT_ID)
		{
			continue;
		}

		UpdatePlayerCommandBuffer(world, objectId);
	}
}

/// <summary>
/// 1体の Player について、古いコマンド候補を消し、今フレーム成立したコマンドを追加する。
/// </summary>
/// <param name="world">対象 Component を取得する World。</param>
/// <param name="objectId">コマンド候補を更新する Player GameObject ID。</param>
void CommandInputSystem::UpdatePlayerCommandBuffer(World& world, GameObjectId objectId)
{
	CommandBufferComponent* commandBuffer = world.GetComponent<CommandBufferComponent>(objectId);
	const InputHistoryComponent* inputHistory = world.GetComponent<InputHistoryComponent>(objectId);
	if (!commandBuffer || !inputHistory || inputHistory->latestFrameIndex < 0)
	{
		return;
	}

	const InputHistoryFrame& latestFrame = inputHistory->frames[inputHistory->latestFrameIndex];
	RemoveExpiredCommands(*commandBuffer, latestFrame.frameNumber);
	RegisterCommandsFromLatestInput(*commandBuffer, *inputHistory);
}

/// <summary>
/// 有効期限を過ぎた先行入力候補を無効化する。
/// </summary>
/// <param name="commandBuffer">更新する CommandBufferComponent。</param>
/// <param name="currentFrameNumber">今フレームの入力履歴番号。</param>
void CommandInputSystem::RemoveExpiredCommands(CommandBufferComponent& commandBuffer, int currentFrameNumber)
{
	for (BufferedCommandInput& command : commandBuffer.commands)
	{
		if (command.valid && command.bufferExpireFrame < currentFrameNumber)
		{
			command = BufferedCommandInput{};
		}
	}
}

/// <summary>
/// 今フレームの攻撃 Trigger を起点に、通常攻撃とコマンド技の候補を登録する。
/// </summary>
/// <param name="commandBuffer">成立したコマンドを書き込む Component。</param>
/// <param name="inputHistory">過去 30F 分の入力履歴。</param>
void CommandInputSystem::RegisterCommandsFromLatestInput(
	CommandBufferComponent& commandBuffer,
	const InputHistoryComponent& inputHistory)
{
	const InputHistoryFrame* latestFrame = GetHistoryFrameFromLatest(inputHistory, 0);
	if (!latestFrame || latestFrame->attackTriggerMask == 0)
	{
		return;
	}

	const int commandAcceptedFrame = latestFrame->frameNumber;

	if (HasButton(latestFrame->attackTriggerMask, InputHistoryAttackMask::AttackA))
	{
		if (MatchHadokenCommand(inputHistory))
		{
			AddBufferedCommand(commandBuffer, "SpecialAttack", commandAcceptedFrame, HadokenPriority);
		}

		AddBufferedCommand(commandBuffer, "AttackA", commandAcceptedFrame, NormalAttackAPriority);
	}

	if (HasButton(latestFrame->attackTriggerMask, InputHistoryAttackMask::AttackB))
	{
		if (MatchShoryuCommand(inputHistory))
		{
			AddBufferedCommand(commandBuffer, "SpecialUpper", commandAcceptedFrame, ShoryuPriority);
		}

		AddBufferedCommand(commandBuffer, "AttackB", commandAcceptedFrame, NormalAttackBPriority);
	}

	if (HasButton(latestFrame->attackTriggerMask, InputHistoryAttackMask::AttackX))
	{
		AddBufferedCommand(commandBuffer, "AttackX", commandAcceptedFrame, NormalAttackXPriority);
	}

	if (HasButton(latestFrame->attackTriggerMask, InputHistoryAttackMask::AttackY))
	{
		AddBufferedCommand(commandBuffer, "AttackY", commandAcceptedFrame, NormalAttackYPriority);
	}
}

/// <summary>
/// コマンド候補をバッファへ追加し、満杯の場合は古い候補か低優先度候補を置き換える。
/// </summary>
/// <param name="commandBuffer">追加先の CommandBufferComponent。</param>
/// <param name="attackSlotId">実行する攻撃スロット ID。</param>
/// <param name="commandAcceptedFrame">コマンドが成立したフレーム番号。</param>
/// <param name="priority">同一フレーム候補の優先度。</param>
void CommandInputSystem::AddBufferedCommand(
	CommandBufferComponent& commandBuffer,
	const std::string& attackSlotId,
	int commandAcceptedFrame,
	int priority)
{
	for (BufferedCommandInput& command : commandBuffer.commands)
	{
		if (!command.valid)
		{
			command.attackSlotId = attackSlotId;
			command.commandAcceptedFrame = commandAcceptedFrame;
			command.bufferExpireFrame = commandAcceptedFrame + CommandBufferFrames;
			command.priority = priority;
			command.valid = true;
			return;
		}
	}

	int replaceIndex = 0;
	for (int index = 1; index < CommandBufferComponent::MaxBufferedCommands; ++index)
	{
		const BufferedCommandInput& current = commandBuffer.commands[index];
		const BufferedCommandInput& replace = commandBuffer.commands[replaceIndex];
		if (current.commandAcceptedFrame < replace.commandAcceptedFrame
			|| (current.commandAcceptedFrame == replace.commandAcceptedFrame && current.priority < replace.priority))
		{
			replaceIndex = index;
		}
	}

	BufferedCommandInput& command = commandBuffer.commands[replaceIndex];
	command.attackSlotId = attackSlotId;
	command.commandAcceptedFrame = commandAcceptedFrame;
	command.bufferExpireFrame = commandAcceptedFrame + CommandBufferFrames;
	command.priority = priority;
	command.valid = true;
}

/// <summary>
/// 波動コマンド 236 が、入力者の向きを基準に成立しているか確認する。
/// </summary>
/// <param name="inputHistory">検索対象の入力履歴。</param>
/// <returns>過去履歴から 236 が見つかれば true。</returns>
bool CommandInputSystem::MatchHadokenCommand(const InputHistoryComponent& inputHistory)
{
	return MatchDirectionCommand(inputHistory, HadokenCommand);
}

/// <summary>
/// 昇竜コマンドが、入力者の向きを基準に成立しているか確認する。
/// </summary>
/// <param name="inputHistory">検索対象の入力履歴。</param>
/// <returns>過去履歴から簡易昇竜入力が見つかれば true。</returns>
bool CommandInputSystem::MatchShoryuCommand(const InputHistoryComponent& inputHistory)
{
	return MatchDirectionCommand(inputHistory, ShoryuCommand);
}

/// <summary>
/// 入力履歴を新しい順に遡り、指定した可変長の相対方向コマンドが成立しているか判定する。
/// </summary>
/// <param name="inputHistory">過去入力を持つ InputHistoryComponent。</param>
/// <param name="relativeCommand">右向き基準の方向コマンド。例: 236 / 41236 / 236236。</param>
/// <returns>コマンドが成立していれば true。</returns>
bool CommandInputSystem::MatchDirectionCommand(
	const InputHistoryComponent& inputHistory,
	std::span<const CommandDirectionStep> relativeCommand)
{
	if (relativeCommand.empty())
	{
		return false;
	}

	int targetIndex = static_cast<int>(relativeCommand.size()) - 1;
	int lastAcceptedDirection = 0;

	for (int offset = 0; offset < inputHistory.storedFrameCount && targetIndex >= 0; ++offset)
	{
		const InputHistoryFrame* frame = GetHistoryFrameFromLatest(inputHistory, offset);
		if (!frame)
		{
			continue;
		}

		const int relativeDirection = ConvertToRelativeDirection(frame->direction, frame->facingDirection);
		if (relativeDirection == 5)
		{
			continue;
		}

		// 同じ方向を押し続けたフレームは 1 回分として扱う。
		// これにより、斜め 1 回だけで 236 などの複数ステップが成立しないようにする。
		if (relativeDirection == lastAcceptedDirection)
		{
			continue;
		}

		if (DoesDirectionMatchStep(relativeDirection, relativeCommand[static_cast<size_t>(targetIndex)]))
		{
			lastAcceptedDirection = relativeDirection;
			--targetIndex;
		}
	}

	return targetIndex < 0;
}

/// <summary>
/// 最新履歴から指定フレーム分だけ古い InputHistoryFrame を取得する。
/// </summary>
/// <param name="inputHistory">ring buffer として保存された入力履歴。</param>
/// <param name="offsetFromLatest">0 が最新、1 が 1 フレーム前。</param>
/// <returns>存在する履歴へのポインタ。範囲外なら nullptr。</returns>
const InputHistoryFrame* CommandInputSystem::GetHistoryFrameFromLatest(
	const InputHistoryComponent& inputHistory,
	int offsetFromLatest)
{
	if (inputHistory.latestFrameIndex < 0
		|| offsetFromLatest < 0
		|| offsetFromLatest >= inputHistory.storedFrameCount)
	{
		return nullptr;
	}

	int frameIndex = inputHistory.latestFrameIndex - offsetFromLatest;
	while (frameIndex < 0)
	{
		frameIndex += InputHistoryComponent::HistoryFrameCount;
	}

	return &inputHistory.frames[frameIndex];
}

/// <summary>
/// 絶対方向入力を、入力フレーム時点の向きを基準にした相対方向へ変換する。
/// </summary>
/// <param name="direction">保存されているテンキー方向。</param>
/// <param name="facingDirection">その入力フレーム時点のプレイヤー向き。</param>
/// <returns>右向き基準へそろえたテンキー方向。</returns>
int CommandInputSystem::ConvertToRelativeDirection(int direction, FacingDirection facingDirection)
{
	if (facingDirection == FacingDirection::Right)
	{
		return direction;
	}

	switch (direction)
	{
	case 1: return 3;
	case 3: return 1;
	case 4: return 6;
	case 6: return 4;
	case 7: return 9;
	case 9: return 7;
	default:
		return direction;
	}
}

/// <summary>
/// 入力方向が、コマンドステップの条件を満たしているか確認する。
/// </summary>
/// <param name="relativeDirection">右向き基準へ変換済みの入力方向。</param>
/// <param name="step">判定するコマンドステップ。</param>
/// <returns>条件を満たしていれば true。</returns>
bool CommandInputSystem::DoesDirectionMatchStep(int relativeDirection, const CommandDirectionStep& step)
{
	if (step.matchMode == CommandDirectionMatchMode::Exact)
	{
		return relativeDirection == step.direction;
	}

	const int inputMask = GetDirectionComponentMask(relativeDirection);
	const int requiredMask = GetDirectionComponentMask(step.direction);
	return requiredMask != 0
		&& (inputMask & requiredMask) == requiredMask;
}

/// <summary>
/// テンキー方向を、上下左右の成分 mask に変換する。
/// </summary>
/// <param name="direction">右向き基準のテンキー方向。</param>
/// <returns>方向が持つ上下左右成分の bit mask。</returns>
int CommandInputSystem::GetDirectionComponentMask(int direction)
{
	switch (direction)
	{
	case 1: return DirectionDown | DirectionBack;
	case 2: return DirectionDown;
	case 3: return DirectionDown | DirectionForward;
	case 4: return DirectionBack;
	case 6: return DirectionForward;
	case 7: return DirectionUp | DirectionBack;
	case 8: return DirectionUp;
	case 9: return DirectionUp | DirectionForward;
	default:
		return 0;
	}
}
