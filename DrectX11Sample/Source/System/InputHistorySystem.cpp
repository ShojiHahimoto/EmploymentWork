#include "System/InputHistorySystem.h"

#include "Input/InputSystem.h"
#include "World/World.h"

#include <cstddef>

using namespace DirectX::SimpleMath;

void InputHistorySystem::Update(World& world)
{
	for (GameObject& object : world.GetGameObjects())
	{
		if (object.tag != GameObjectTag::Player)
		{
			continue;
		}

		UpdateInputHistory(world, object.id);
	}
}

void InputHistorySystem::UpdateInputHistory(World& world, GameObjectId objectId)
{
	InputHistoryComponent* inputHistory = world.GetComponent<InputHistoryComponent>(objectId);
	if (!inputHistory)
	{
		return;
	}

	// 現段階では 1P の入力だけを格闘ゲーム用履歴へ変換して保存する。
	// 将来は PlayerIndexComponent などで GameObject ごとの playerIndex を分ける。
	const Input::PlayerInputState& inputState = Input::InputSystem::GetPlayerInputState(0);
	inputHistory->latestFrameIndex = 0;
	inputHistory->frames[inputHistory->latestFrameIndex] = BuildHistoryFrame(inputState);
}

InputHistoryFrame InputHistorySystem::BuildHistoryFrame(const Input::PlayerInputState& inputState)
{
	InputHistoryFrame frame;

	const Input::InputActionState& move =
		inputState.actions[static_cast<size_t>(Input::InputActionId::Move)];

	frame.direction = ConvertMoveAxisToDirection(move.value.axis);
	const int previousDirection = ConvertMoveAxisToDirection(move.previousValue.axis);

	frame.lightAttack = CopyButtonState(inputState.actions[static_cast<size_t>(Input::InputActionId::LightAttack)]);
	frame.mediumAttack = CopyButtonState(inputState.actions[static_cast<size_t>(Input::InputActionId::MediumAttack)]);
	frame.heavyAttack = CopyButtonState(inputState.actions[static_cast<size_t>(Input::InputActionId::HeavyAttack)]);
	// バトル操作では 7 / 8 / 9 方向をジャンプ入力として扱う。
	// 今後、前ジャンプ・垂直ジャンプ・バックジャンプに分ける場合も、
	// direction には 7 / 8 / 9 の区別を残したまま jump の Trigger / Press / Release を参照できる。
	frame.jump = BuildJumpDirectionState(frame.direction, previousDirection);
	frame.guard = CopyButtonState(inputState.actions[static_cast<size_t>(Input::InputActionId::Guard)]);

	return frame;
}

int InputHistorySystem::ConvertMoveAxisToDirection(const Vector2& moveAxis)
{
	constexpr float DirectionThreshold = 0.5f;

	int x = 0;
	if (moveAxis.x <= -DirectionThreshold)
	{
		x = -1;
	}
	else if (moveAxis.x >= DirectionThreshold)
	{
		x = 1;
	}

	int y = 0;
	if (moveAxis.y <= -DirectionThreshold)
	{
		y = -1;
	}
	else if (moveAxis.y >= DirectionThreshold)
	{
		y = 1;
	}

	if (y > 0)
	{
		return 8 + x;
	}

	if (y < 0)
	{
		return 2 + x;
	}

	return 5 + x;
}

InputButtonHistoryState InputHistorySystem::BuildJumpDirectionState(int currentDirection, int previousDirection)
{
	const bool currentJump = IsJumpDirection(currentDirection);
	const bool previousJump = IsJumpDirection(previousDirection);

	InputButtonHistoryState button;
	button.trigger = !previousJump && currentJump;
	button.press = currentJump;
	button.release = previousJump && !currentJump;
	return button;
}

bool InputHistorySystem::IsJumpDirection(int direction)
{
	return direction == 7 || direction == 8 || direction == 9;
}

InputButtonHistoryState InputHistorySystem::CopyButtonState(const Input::InputActionState& actionState)
{
	InputButtonHistoryState button;
	button.trigger = actionState.trigger;
	button.press = actionState.press;
	button.release = actionState.release;
	return button;
}
