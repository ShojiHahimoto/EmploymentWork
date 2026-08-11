#include "System/PlayerControlSystem.h"

#include "Component/CharacterParameterComponent.h"
#include "System/Debugger.h"
#include "Component/TransformComponent.h"
#include "System/MovementSystem.h"
#include "System/TransformSystem.h"
#include "World/World.h"

using namespace DirectX::SimpleMath;

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

/// <summary>
/// 1体の Player に対して、今フレームの行動結果を Velocity に反映する。
/// </summary>
/// <param name="world">対象 Component を取得する World。</param>
/// <param name="objectId">行動処理を行う Player GameObject の ID。</param>
void PlayerControlSystem::UpdatePlayer(World& world, GameObjectId objectId)
{
	// 必要コンポーネントを取得
	VelocityComponent* velocity = world.GetComponent<VelocityComponent>(objectId);
	StateComponent* state = world.GetComponent<StateComponent>(objectId);
	CharacterParameterComponent* characterParameter = world.GetComponent<CharacterParameterComponent>(objectId);
	TransformComponent* transform = world.GetComponent<TransformComponent>(objectId);

	// 必要コンポーネントが不足している場合更新しない
	if (!world.GetTransform(objectId) || !velocity || !state || !characterParameter || !transform)
	{
		return;
	}

	const PlayerControlFrameResult result = ExecuteCurrentAction(*state, characterParameter->parameter);
	ApplyFrameResult(*velocity, result);
	ApplyPlayerDirection(*state, *transform);
}

/// <summary>
/// 現在の PlayerActionState を読み、今フレームに Velocity へ書き込む値だけを作る。
/// </summary>
/// <param name="state">StateUpdateSystem が確定した Player の状態。</param>
/// <param name="parameter">キャラクターごとの移動・ジャンプ調整値。</param>
/// <returns>Velocity へ反映する今フレームの行動結果。</returns>
PlayerControlFrameResult PlayerControlSystem::ExecuteCurrentAction(
	const StateComponent& state,
	const CharacterParameterData& parameter)
{
	PlayerControlFrameResult result;

	// プレイヤーの現在向きによって、歩きなどの地上行動方向を決めるための符号。
	int dirIndex = 1;
	if (state.facingDirection == FacingDirection::Left)
	{
		dirIndex = -1;
	}

	// ジャンプの横速度は、実ジャンプ開始時の向きではなくジャンプ入力時の向きを基準に固定する。
	int actionStartDirIndex = 1;
	if (state.actionStartFacingDirection == FacingDirection::Left)
	{
		actionStartDirIndex = -1;
	}

	switch (state.currentActionState)
	{
	case PlayerActionState::FrontWalk:
		// 歩き状態の間だけ、テンキー方向の横成分を歩き速度として毎フレーム上書きする。
		// 7 / 9 は今後ジャンプ方向としても使うため、横成分はここで残しておく。
		result.horizontalVelocity = dirIndex * parameter.forwardWalkSpeed;
		break;

	case PlayerActionState::BackWalk:
		// 歩き状態の間だけ、テンキー方向の横成分を歩き速度として毎フレーム上書きする。
		// 7 / 9 は今後ジャンプ方向としても使うため、横成分はここで残しておく。
		result.horizontalVelocity = -dirIndex * parameter.backwardWalkSpeed;
		break;

	case PlayerActionState::VerticalJumpStartup:
	case PlayerActionState::FrontJumpStartup:
	case PlayerActionState::BackJumpStartup:
		// ジャンプ移行中はまだ地上行動として扱う。
		// この間に攻撃へ上書きできるよう、実ジャンプの初速は入れず足元も滑らせない。
		result.horizontalVelocity = 0.0f;
		result.verticalVelocity = 0.0f;
		result.setVerticalVelocity = true;
		break;

	case PlayerActionState::VerticalJump:
		// Jump に入った最初のフレームだけ上方向の初速を設定する。
		// それ以降の上昇低下や重力は MovementSystem が毎フレーム処理する。
		result.setHorizontalVelocity = false;
		if (state.actionFrame == 0)
		{
			result.verticalVelocity = parameter.jumpInitialVelocity;
			result.setVerticalVelocity = true;
		}
		break;

	case PlayerActionState::FrontJump:
		// Jump に入った最初のフレームだけ上方向の初速を設定する。
		// それ以降の上昇低下や重力は MovementSystem が毎フレーム処理する。
		result.setHorizontalVelocity = false;
		if (state.actionFrame == 0)
		{
			result.verticalVelocity = parameter.jumpInitialVelocity;
			result.horizontalVelocity = parameter.frontJumpHorizontalVelocity * actionStartDirIndex;
			result.setHorizontalVelocity = true;
			result.setVerticalVelocity = true;
		}
		break;

	case PlayerActionState::BackJump:
		// Jump に入った最初のフレームだけ上方向の初速を設定する。
		// それ以降の上昇低下や重力は MovementSystem が毎フレーム処理する。
		result.setHorizontalVelocity = false;
		if (state.actionFrame == 0)
		{
			result.verticalVelocity = parameter.jumpInitialVelocity;
			result.horizontalVelocity = parameter.backJumpHorizontalVelocity * actionStartDirIndex;
			result.setHorizontalVelocity = true;
			result.setVerticalVelocity = true;
		}
		break;

	case PlayerActionState::AirAttack:
	case PlayerActionState::AirHitstun:
		result.setHorizontalVelocity = false;
		break;

	case PlayerActionState::Idle:
	case PlayerActionState::GroundAttack:
	case PlayerActionState::LandingRecovery:
	case PlayerActionState::Hitstun:
	case PlayerActionState::Guardstun:
	case PlayerActionState::Down:
	case PlayerActionState::WakeUp:
		// 現段階の攻撃・着地硬直・地上被弾・ガード・ダウン系は、横移動を止める。
		// 攻撃移動やノックバックを入れる場合は、各 ActionState の処理としてここから分岐を増やす。
		//result.horizontalVelocity = 0.0f;
		break;

	case PlayerActionState::Fall:
	default:
		// 落下中は新しい横速度をここで作らない。
		// 空中制御を入れる場合は Fall / Jump の扱いを明示して追加する。
		result.setHorizontalVelocity = false;
		break;
	}

	return result;
}

/// <summary>
/// テンキー方向から横入力だけを取り出す。
/// </summary>
/// <param name="direction">判定するテンキー方向。</param>
/// <returns>左なら -1、右なら 1、横入力なしなら 0。</returns>
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

/// <summary>
/// 行動処理で作った結果を VelocityComponent へ反映する。
/// </summary>
/// <param name="velocity">変更する VelocityComponent。</param>
/// <param name="result">今フレームの行動処理結果。</param>
void PlayerControlSystem::ApplyFrameResult(
	VelocityComponent& velocity,
	const PlayerControlFrameResult& result)
{
	if (result.setHorizontalVelocity)
	{
		MovementSystem::SetVelocityX(velocity, result.horizontalVelocity);
	}

	if (result.setVerticalVelocity)
	{
		MovementSystem::SetVelocityY(velocity, result.verticalVelocity);
	}
}

void PlayerControlSystem::ApplyPlayerDirection(const StateComponent& state, TransformComponent& transform)
{
	if (state.facingDirection == FacingDirection::Right)
	{
		TransformSystem::SetLocalEulerRotationDegrees(transform, Vector3(0, -90, 0));
	}
	else
	{
		TransformSystem::SetLocalEulerRotationDegrees(transform, Vector3(0, 90, 0));
	}
}
