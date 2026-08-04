#pragma once

#include "Component/InputHistoryComponent.h"
#include "Core/GameObject.h"
#include "Data/AttackData.h"

#include <span>
#include <string>

class World;
struct CharacterAttackDataComponent;
struct CommandBufferComponent;
struct StateComponent;

enum class CommandDirectionMatchMode
{
	Exact,
	ContainsComponents,
};

struct CommandDirectionStep
{
	// 右向き基準のテンキー方向。ContainsComponents では、この方向が持つ上下左右成分だけを見る。
	int direction = 5;
	CommandDirectionMatchMode matchMode = CommandDirectionMatchMode::ContainsComponents;
};

class CommandInputSystem
{
public:
	// InputHistoryComponent からコマンド成立を判定し、CommandBufferComponent に候補を保存する。
	static void Update(World& world);

	// 先行入力として保持する共通猶予フレーム。
	// 技ごとではなく一括調整できるよう、現段階では System 側の固定値にする。
	static constexpr int CommandBufferFrames = 5;

private:
	static void UpdatePlayerCommandBuffer(World& world, GameObjectId objectId);
	static void RemoveExpiredCommands(CommandBufferComponent& commandBuffer, int currentFrameNumber);
	static void RegisterCommandsFromLatestInput(
		CommandBufferComponent& commandBuffer,
		const InputHistoryComponent& inputHistory,
		const CharacterAttackDataComponent& characterAttackData,
		const StateComponent& state);
	static void AddBufferedCommand(
		CommandBufferComponent& commandBuffer,
		const std::string& attackSlotId,
		int commandAcceptedFrame,
		int priority,
		AttackUsableState usableState);
	static bool MatchCommand(const InputHistoryComponent& inputHistory, AttackCommandId commandId);
	static bool MatchFullRotateCommand(const InputHistoryComponent& inputHistory);
	static bool MatchDirectionCommand(
		const InputHistoryComponent& inputHistory,
		std::span<const CommandDirectionStep> relativeCommand);
	static const InputHistoryFrame* GetHistoryFrameFromLatest(const InputHistoryComponent& inputHistory, int offsetFromLatest);
	static int ConvertToRelativeDirection(int direction, FacingDirection facingDirection);
	static bool DoesDirectionMatchStep(int relativeDirection, const CommandDirectionStep& step);
	static int GetDirectionComponentMask(int direction);
	static uint32_t GetAttackButtonMask(AttackButtonId button);
	static int GetNormalAttackPriority(AttackButtonId button);
	static int GetCommandPriority(AttackCommandId commandId);
	static bool IsAttackUsableInState(AttackUsableState usableState, const StateComponent& state);
};
