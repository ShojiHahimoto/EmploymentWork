#include "System/MovementSystem.h"

#include "Component/StateComponent.h"
#include "Component/TransformComponent.h"
#include "Component/VelocityComponent.h"
#include "System/TransformSystem.h"
#include "World/World.h"

using namespace DirectX::SimpleMath;

void MovementSystem::Update(World& world)
{
	ApplyAirGravity(world);
	ApplyVelocityToTransform(world);
	ResolveTemporaryGround(world);
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
		StateComponent* state = world.GetComponent<StateComponent>(object.id);
		VelocityComponent* velocity = world.GetComponent<VelocityComponent>(object.id);
		if (!transform || !state || !velocity)
		{
			continue;
		}

		if (!ShouldApplyGravity(*transform, *state))
		{
			continue;
		}

		state->isGrounded = false;
		const float gravity = velocity->velocity.y > 0.0f
			? RiseGravityPerFrame
			: FallGravityPerFrame;
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
/// 仮の地面判定として y <= 0 を接地扱いに補正する。
/// </summary>
/// <param name="world">接地補正対象の Component を取得する World。</param>
void MovementSystem::ResolveTemporaryGround(World& world)
{
	for (GameObject& object : world.GetGameObjects())
	{
		TransformComponent* transform = world.GetTransform(object.id);
		StateComponent* state = world.GetComponent<StateComponent>(object.id);
		VelocityComponent* velocity = world.GetComponent<VelocityComponent>(object.id);
		if (!transform || !state || !velocity)
		{
			continue;
		}

		Vector3 position = TransformSystem::GetLocalPosition(*transform);
		if (position.y <= 0.0f)
		{
			position.y = 0.0f;
			TransformSystem::SetLocalPosition(*transform, position);
			SetVelocityY(*velocity, 0.0f);
			state->isGrounded = true;
			continue;
		}

		state->isGrounded = false;
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
	if (state.currentActionState == PlayerActionState::Jump
		|| state.currentActionState == PlayerActionState::Fall)
	{
		return true;
	}

	if (!state.isGrounded)
	{
		return true;
	}

	return TransformSystem::GetLocalPosition(transform).y > 0.0f;
}
