#include "System/InputHistorySystem.h"

#include "Input/InputSystem.h"
#include "World/World.h"

#include <cstddef>

using namespace DirectX::SimpleMath;

/// <summary>
/// Player タグを持つ GameObject の入力履歴を、今フレームの InputSystem 結果から更新する。
/// </summary>
/// <param name="world">更新対象の GameObject と Component を保持している World。</param>
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

/// <summary>
/// 指定された Player GameObject の InputHistoryComponent に、1 フレーム分の入力履歴を書き込む。
/// </summary>
/// <param name="world">対象 Component を取得するための World。</param>
/// <param name="objectId">入力履歴を更新する GameObject の ID。</param>
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

/// <summary>
/// InputSystem の PlayerInputState を、格闘ゲーム用の InputHistoryFrame に変換する。
/// </summary>
/// <param name="inputState">InputSystem が今フレーム確定した 1 Player 分の入力状態。</param>
/// <returns>テンキー方向、攻撃、ジャンプ、ガードをまとめた今フレームの入力履歴。</returns>
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

/// <summary>
/// 2D 移動入力を格闘ゲーム用のテンキー方向 1〜9 に変換する。
/// </summary>
/// <param name="moveAxis">InputSystem の Move Action が持つ 2D 軸入力。</param>
/// <returns>未入力を 5 とするテンキー方向。斜め入力も 1 / 3 / 7 / 9 として返す。</returns>
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

/// <summary>
/// 現在方向と前フレーム方向から、ジャンプ入力の Trigger / Press / Release を作る。
/// </summary>
/// <param name="currentDirection">今フレームのテンキー方向。</param>
/// <param name="previousDirection">前フレームのテンキー方向。</param>
/// <returns>7 / 8 / 9 方向をジャンプ入力として扱ったボタン相当の入力状態。</returns>
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

/// <summary>
/// テンキー方向がジャンプ方向かどうかを判定する。
/// </summary>
/// <param name="direction">判定するテンキー方向。</param>
/// <returns>7 / 8 / 9 のいずれかなら true、それ以外なら false。</returns>
bool InputHistorySystem::IsJumpDirection(int direction)
{
	return direction == 7 || direction == 8 || direction == 9;
}

/// <summary>
/// InputSystem が判定済みの Button Action 状態を、入力履歴用の状態へコピーする。
/// </summary>
/// <param name="actionState">InputSystem 側の Trigger / Press / Release を持つ Action 状態。</param>
/// <returns>InputHistoryComponent に保存するためのボタン入力状態。</returns>
InputButtonHistoryState InputHistorySystem::CopyButtonState(const Input::InputActionState& actionState)
{
	InputButtonHistoryState button;
	button.trigger = actionState.trigger;
	button.press = actionState.press;
	button.release = actionState.release;
	return button;
}
