#include "System/MovementSystem.h"

#include "Component/CharacterParameterComponent.h"
#include "Component/CharacterAttackDataComponent.h"
#include "Component/HitBoxComponent.h"
#include "Component/StateComponent.h"
#include "Component/TransformComponent.h"
#include "Component/VelocityComponent.h"
#include "System/TransformSystem.h"
#include "World/World.h"

#include <algorithm>

using namespace DirectX::SimpleMath;

void MovementSystem::Update(World& world)
{
	ApplyAirGravity(world);
	ApplyAttackMovement(world);
	ApplyVelocityToTransform(world);
}

/// <summary>
/// VelocityComponent の移動量をまとめて上書きする。
/// </summary>
/// <param name="velocity">変更する VelocityComponent。</param>
/// <param name="value">設定する 1 フレーム分の移動量。</param>
void MovementSystem::SetVelocity(VelocityComponent& velocity, const Vector3& value)
{
	velocity.velocity = value;
}

/// <summary>
/// VelocityComponent の X 成分だけを上書きする。
/// </summary>
/// <param name="velocity">変更する VelocityComponent。</param>
/// <param name="value">設定する X 方向の移動量。</param>
void MovementSystem::SetVelocityX(VelocityComponent& velocity, float value)
{
	velocity.velocity.x = value;
}

/// <summary>
/// VelocityComponent の Y 成分だけを上書きする。
/// </summary>
/// <param name="velocity">変更する VelocityComponent。</param>
/// <param name="value">設定する Y 方向の移動量。</param>
void MovementSystem::SetVelocityY(VelocityComponent& velocity, float value)
{
	velocity.velocity.y = value;
}

/// <summary>
/// VelocityComponent の Z 成分だけを上書きする。
/// </summary>
/// <param name="velocity">変更する VelocityComponent。</param>
/// <param name="value">設定する Z 方向の移動量。</param>
void MovementSystem::SetVelocityZ(VelocityComponent& velocity, float value)
{
	velocity.velocity.z = value;
}

/// <summary>
/// VelocityComponent に外力や技移動などの追加移動量を加算する。
/// </summary>
/// <param name="velocity">変更する VelocityComponent。</param>
/// <param name="value">加算する 1 フレーム分の移動量。</param>
void MovementSystem::AddVelocity(VelocityComponent& velocity, const Vector3& value)
{
	velocity.velocity += value;
}

/// <summary>
/// VelocityComponent の Y 成分に移動量を加算する。
/// </summary>
/// <param name="velocity">変更する VelocityComponent。</param>
/// <param name="value">加算する Y 方向の移動量。</param>
void MovementSystem::AddVelocityY(VelocityComponent& velocity, float value)
{
	velocity.velocity.y += value;
}

/// <summary>
/// 空中またはジャンプ・落下中の Player に重力を加算する。
/// </summary>
/// <param name="world">重力対象の Component を取得する World。</param>
void MovementSystem::ApplyAirGravity(World& world)
{
	for (GameObject& object : world.GetGameObjects())
	{
		TransformComponent* transform = world.GetTransform(object.id);
		CharacterParameterComponent* characterParameter = world.GetComponent<CharacterParameterComponent>(object.id);
		StateComponent* state = world.GetComponent<StateComponent>(object.id);
		VelocityComponent* velocity = world.GetComponent<VelocityComponent>(object.id);
		if (!transform || !characterParameter || !state || !velocity)
		{
			continue;
		}

		if (!ShouldApplyGravity(*transform, *state))
		{
			continue;
		}

		state->isGrounded = false;
		const float gravity = velocity->velocity.y > 0.0f
			? characterParameter->parameter.riseGravityPerFrame
			: characterParameter->parameter.fallGravityPerFrame;
		AddVelocityY(*velocity, gravity);
	}
}

/// <summary>
/// VelocityComponent の移動量を TransformComponent のローカル座標へ反映する。
/// </summary>
/// <param name="world">移動対象の Component を取得する World。</param>
void MovementSystem::ApplyVelocityToTransform(World& world)
{
	for (GameObject& object : world.GetGameObjects())
	{
		TransformComponent* transform = world.GetTransform(object.id);
		VelocityComponent* velocity = world.GetComponent<VelocityComponent>(object.id);
		if (!transform || !velocity)
		{
			continue;
		}

		// Velocity は 1 フレームあたりの移動量として扱い、localPosition へ即時反映する。
		TransformSystem::SetLocalPosition(
			*transform,
			TransformSystem::GetLocalPosition(*transform) + velocity->velocity);
	}
}

/// <summary>
/// 攻撃中の movementKeys から今フレームの実移動差分を計算し、Transform へ反映する。
/// </summary>
/// <param name="world">攻撃中 Player の Component を取得する World。</param>
void MovementSystem::ApplyAttackMovement(World& world)
{
	for (GameObject& object : world.GetGameObjects())
	{
		TransformComponent* transform = world.GetTransform(object.id);
		StateComponent* state = world.GetComponent<StateComponent>(object.id);
		HitBoxComponent* hitBox = world.GetComponent<HitBoxComponent>(object.id);
		CharacterAttackDataComponent* attackData = world.GetComponent<CharacterAttackDataComponent>(object.id);
		if (!transform || !state || !hitBox || !attackData)
		{
			continue;
		}

		if (state->currentActionState != PlayerActionState::GroundAttack
			&& state->currentActionState != PlayerActionState::AirAttack)
		{
			continue;
		}

		const CharacterAssignedAttackData* assignedAttack = FindAssignedAttack(attackData, hitBox->currentAttack.slotId);
		if (!assignedAttack || assignedAttack->attack.movementKeys.empty())
		{
			continue;
		}

		const Vector2 currentOffset = SampleAttackMovementOffset(assignedAttack->attack, state->actionFrame);
		const Vector2 previousOffset = state->actionFrame > 0
			? SampleAttackMovementOffset(assignedAttack->attack, state->actionFrame - 1)
			: Vector2::Zero;
		const Vector2 delta = currentOffset - previousOffset;
		if (delta.LengthSquared() <= 0.000001f)
		{
			continue;
		}

		const float facingSign = state->actionStartFacingDirection == FacingDirection::Right ? 1.0f : -1.0f;
		Vector3 position = TransformSystem::GetLocalPosition(*transform);
		position.x += delta.x * facingSign;
		position.y += delta.y;
		TransformSystem::SetLocalPosition(*transform, position);
	}
}

/// <summary>
/// 指定 Player に今フレーム重力を適用するべきか判定する。
/// </summary>
/// <param name="transform">現在位置を確認する TransformComponent。</param>
/// <param name="state">接地状態と ActionState を確認する StateComponent。</param>
/// <returns>重力を加算する必要があれば true。</returns>
bool MovementSystem::ShouldApplyGravity(const TransformComponent& transform, const StateComponent& state)
{
	//if (state.currentActionState == PlayerActionState::VerticalJump
	//	|| state.currentActionState == PlayerActionState::FrontJump
	//	|| state.currentActionState == PlayerActionState::BackJump
	//	|| state.currentActionState == PlayerActionState::Fall
	//	|| state.currentActionState == PlayerActionState::AirAttack)
	if(!state.isGrounded)
	{
		return true;
	}

	return false;

	//if (!state.isGrounded)
	//{
	//	return true;
	//}

	//return TransformSystem::GetLocalPosition(transform).y > 0.0f;
}

/// <summary>
/// 攻撃開始位置から見た相対移動量を、movementKeys から線形補間して取得する。
/// </summary>
/// <param name="attack">参照する AttackData。</param>
/// <param name="actionFrame">攻撃開始を 0 とする現在フレーム。</param>
/// <returns>攻撃開始地点からの相対移動量。キーがない場合は 0。</returns>
Vector2 MovementSystem::SampleAttackMovementOffset(const AttackData& attack, int actionFrame)
{
	if (attack.movementKeys.empty() || actionFrame < 0)
	{
		return Vector2::Zero;
	}

	const AttackMovementKeyData* previousKey = nullptr;
	const AttackMovementKeyData* nextKey = nullptr;
	for (const AttackMovementKeyData& key : attack.movementKeys)
	{
		if (key.frame <= actionFrame)
		{
			previousKey = &key;
		}

		if (key.frame >= actionFrame)
		{
			nextKey = &key;
			break;
		}
	}

	if (!previousKey && nextKey)
	{
		if (nextKey->frame <= 0)
		{
			return nextKey->offset;
		}

		const float rate = std::clamp(
			static_cast<float>(actionFrame) / static_cast<float>(nextKey->frame),
			0.0f,
			1.0f);
		return Vector2::Lerp(Vector2::Zero, nextKey->offset, rate);
	}
	if (previousKey && !nextKey)
	{
		return previousKey->offset;
	}
	if (previousKey && nextKey && previousKey->frame != nextKey->frame)
	{
		const float rate = std::clamp(
			static_cast<float>(actionFrame - previousKey->frame) / static_cast<float>(nextKey->frame - previousKey->frame),
			0.0f,
			1.0f);
		return Vector2::Lerp(previousKey->offset, nextKey->offset, rate);
	}
	if (previousKey)
	{
		return previousKey->offset;
	}

	return Vector2::Zero;
}

/// <summary>
/// CharacterAttackDataComponent から現在攻撃スロットに対応する技データを探す。
/// </summary>
/// <param name="attackData">検索対象のキャラクター技データ。</param>
/// <param name="attackSlotId">現在実行中の攻撃スロット ID。</param>
/// <returns>見つかった割り当て技。存在しない場合は nullptr。</returns>
const CharacterAssignedAttackData* MovementSystem::FindAssignedAttack(
	const CharacterAttackDataComponent* attackData,
	const std::string& attackSlotId)
{
	if (!attackData || attackSlotId.empty())
	{
		return nullptr;
	}

	for (const CharacterAssignedAttackData& assignedAttack : attackData->attacks)
	{
		if (assignedAttack.slotId == attackSlotId)
		{
			return &assignedAttack;
		}
	}

	return nullptr;
}
