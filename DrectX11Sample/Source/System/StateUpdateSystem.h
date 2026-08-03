#pragma once

#include "Component/InputHistoryComponent.h"
#include "Component/StateComponent.h"
#include "Core/GameObject.h"

#include <string>

class World;
struct CharacterAttackDataComponent;
struct CommandBufferComponent;
struct HitBoxComponent;
struct VelocityComponent;

struct PlayerActionDecision
{
	PlayerActionState nextActionState = PlayerActionState::Idle;

	// Same PlayerActionState can be restarted by a new accepted input.
	// Example: a finished attack accepts another attack and actionFrame must return to 0.
	bool restartAction = false;

	// 攻撃へ遷移する場合に、CharacterAttackDataComponent のどの slotId を使うかを指定する。
	std::string attackSlotId;

	// CommandBufferComponent から選んだ候補を、攻撃開始時に消費するための識別情報。
	bool consumeCommand = false;
	int commandAcceptedFrame = -1;
};

class StateUpdateSystem
{
public:
	// Updates PlayerActionState and actionFrame from input history and current player conditions.
	static void Update(World& world);

private:
	static void UpdatePlayerState(World& world, GameObjectId objectId);
	static PlayerActionDecision DecideNextAction(
		const StateComponent& state,
		const VelocityComponent& velocity,
		const InputHistoryFrame& inputFrame,
		const CommandBufferComponent* commandBuffer);
	static PlayerActionDecision DecideBufferedAttack(
		const StateComponent& state,
		const InputHistoryFrame& inputFrame,
		const CommandBufferComponent* commandBuffer);
	static PlayerActionDecision DecideJumpStartupAction(
		const StateComponent& state,
		const InputHistoryFrame& inputFrame,
		const CommandBufferComponent* commandBuffer);
	static PlayerActionDecision DecideNeutralAction(
		const StateComponent& state,
		const VelocityComponent& velocity,
		const InputHistoryFrame& inputFrame,
		const CommandBufferComponent* commandBuffer);
	static bool HasHorizontalMoveDirection(int direction);
	static bool IsJumpStartupAction(PlayerActionState actionState);
	static PlayerActionState ConvertJumpStartupToJump(PlayerActionState actionState);
	static bool TrySelectBufferedAttack(
		const CommandBufferComponent* commandBuffer,
		int currentFrameNumber,
		std::string& outAttackSlotId,
		int& outCommandAcceptedFrame);
	static bool IsLockedAction(PlayerActionState actionState);
	static bool IsActionFinished(const StateComponent& state);
	static int CalculateAttackTotalFrames(const CharacterAttackDataComponent* attackData, const std::string& attackSlotId);
	static bool CanCancelAction(const StateComponent& state);
	static void ApplyActionState(
		StateComponent& state,
		HitBoxComponent* hitBox,
		const CharacterAttackDataComponent* attackData,
		CommandBufferComponent* commandBuffer,
		const PlayerActionDecision& decision);
	static void ConsumeBufferedCommand(CommandBufferComponent* commandBuffer, const PlayerActionDecision& decision);
};
