#pragma once

#include "Component/InputHistoryComponent.h"
#include "Core/GameObject.h"
#include "Input/InputTypes.h"

class World;

class InputHistorySystem
{
public:
	// InputSystem で確定済みの入力を、格闘ゲーム用の入力履歴に変換して保存する。
	static void Update(World& world);

private:
	static void UpdateInputHistory(World& world, GameObjectId objectId, int playerIndex);
	static InputHistoryFrame BuildHistoryFrame(
		const Input::PlayerInputState& inputState,
		FacingDirection facingDirection,
		int frameNumber,
		const InputHistoryFrame* previousFrame);
	static void UpdateDisplayHistory(InputHistoryComponent& inputHistory, const InputHistoryFrame& frame);
	static bool IsSameDisplayInput(const InputDisplayHistoryEntry& entry, const InputHistoryFrame& frame);
	static int ConvertMoveAxisToDirection(const DirectX::SimpleMath::Vector2& moveAxis);
	static InputButtonHistoryState BuildJumpDirectionState(int currentDirection, int previousDirection);
	static bool IsJumpDirection(int direction);
	static InputButtonHistoryState CopyButtonState(const Input::InputActionState& actionState);
	static uint32_t BuildAttackMask(
		const InputButtonHistoryState& attackA,
		const InputButtonHistoryState& attackB,
		const InputButtonHistoryState& attackX,
		const InputButtonHistoryState& attackY,
		bool InputButtonHistoryState::* stateMember);
};
