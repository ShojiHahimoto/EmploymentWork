#include "System/PlayerControlSystem.h"

#include "Component/TransformComponent.h"
#include "System/MovementSystem.h"
#include "World/World.h"

// Player タグの GameObject だけを対象に、確定済み ActionState の実行処理を回す。
// 「どの ActionState になるか」は StateUpdateSystem が先に決めるため、ここでは状態を決定しない。
void PlayerControlSystem::Update(World& world)
{
	for (GameObject& object : world.GetGameObjects())
	{
		if (object.tag != GameObjectTag::Player)
		{
			continue;
		}

		UpdatePlayer(world, object.id);
	}
}

// 1体の Player に対して、今フレームの行動結果を Velocity に反映する。
// Transform は存在確認だけ行い、実際の座標更新は後続の MovementSystem に任せる。
void PlayerControlSystem::UpdatePlayer(World& world, GameObjectId objectId)
{
	VelocityComponent* velocity = world.GetComponent<VelocityComponent>(objectId);
	StateComponent* state = world.GetComponent<StateComponent>(objectId);
	InputHistoryComponent* inputHistory = world.GetComponent<InputHistoryComponent>(objectId);
	if (!world.GetTransform(objectId) || !velocity || !state || !inputHistory)
	{
		return;
	}

	const PlayerControlFrameResult result = ExecuteCurrentAction(*state, *inputHistory);
	ApplyFrameResult(*velocity, result);
}

// 現在の PlayerActionState を読み、今フレームに Velocity へ書き込む値だけを作る。
// 入力から状態を選ぶ処理はここでは行わず、StateUpdateSystem が決めた状態ごとの挙動だけを書く。
PlayerControlFrameResult PlayerControlSystem::ExecuteCurrentAction(
	const StateComponent& state,
	const InputHistoryComponent& inputHistory)
{
	PlayerControlFrameResult result;
	const InputHistoryFrame& inputFrame = inputHistory.frames[inputHistory.latestFrameIndex];

	switch (state.currentActionState)
	{
	case PlayerActionState::Walk:
		// 歩き状態の間だけ、テンキー方向の横成分を歩き速度として毎フレーム上書きする。
		// 7 / 9 は今後ジャンプ方向としても使うため、横成分はここで残しておく。
		result.horizontalVelocity = GetHorizontalInputFromDirection(inputFrame.direction) * MoveSpeedPerFrame;
		break;

	case PlayerActionState::Jump:
		// Jump に入った最初のフレームだけ上方向の初速を設定する。
		// それ以降の上昇低下や重力は MovementSystem が毎フレーム処理する。
		if (state.actionFrame == 0)
		{
			result.verticalVelocity = JumpInitialVelocity;
			result.setVerticalVelocity = true;
		}
		break;

	case PlayerActionState::Idle:
	case PlayerActionState::GroundAttack:
	case PlayerActionState::AirAttack:
	case PlayerActionState::Hitstun:
		// 現段階の攻撃・被弾は仮挙動として横移動を止める。
		// 攻撃移動やノックバックを入れる場合は、各 ActionState の処理としてここから分岐を増やす。
		result.horizontalVelocity = 0.0f;
		break;

	case PlayerActionState::Fall:
	default:
		// 落下中は新しい横速度をここで作らない。
		// 空中制御を入れる場合は Fall / Jump の扱いを明示して追加する。
		break;
	}

	return result;
}

// テンキー方向から横入力だけを取り出す。
// 1 / 4 / 7 は左、3 / 6 / 9 は右、2 / 5 / 8 は横入力なしとして扱う。
float PlayerControlSystem::GetHorizontalInputFromDirection(int direction)
{
	if (direction == 1 || direction == 4 || direction == 7)
	{
		return -1.0f;
	}

	if (direction == 3 || direction == 6 || direction == 9)
	{
		return 1.0f;
	}

	return 0.0f;
}

// 行動処理で作った結果を VelocityComponent へ反映する。
// X は行動状態に応じて毎フレーム上書きし、Y はジャンプ初速など必要な時だけ上書きする。
void PlayerControlSystem::ApplyFrameResult(
	VelocityComponent& velocity,
	const PlayerControlFrameResult& result)
{
	MovementSystem::SetVelocityX(velocity, result.horizontalVelocity);

	if (result.setVerticalVelocity)
	{
		MovementSystem::SetVelocityY(velocity, result.verticalVelocity);
	}
}
