#pragma once

#include <SimpleMath.h>

struct StateComponent;
struct TransformComponent;
struct VelocityComponent;
class World;

class MovementSystem
{
public:
	// 確定済み Velocity を Transform に反映する。
	static void Update(World& world);

	// 上書きと加算を分け、操作系と外力系を混ぜずに扱えるようにする。
	static void SetVelocity(VelocityComponent& velocity, const DirectX::SimpleMath::Vector3& value);
	static void SetVelocityX(VelocityComponent& velocity, float value);
	static void SetVelocityY(VelocityComponent& velocity, float value);
	static void SetVelocityZ(VelocityComponent& velocity, float value);
	static void AddVelocity(VelocityComponent& velocity, const DirectX::SimpleMath::Vector3& value);
	static void AddVelocityY(VelocityComponent& velocity, float value);

private:
	static constexpr float RiseGravityPerFrame = -0.012f;
	static constexpr float FallGravityPerFrame = -0.020f;

	static void ApplyAirGravity(World& world);
	static void ApplyVelocityToTransform(World& world);
	static void ResolveTemporaryGround(World& world);
	static bool ShouldApplyGravity(const TransformComponent& transform, const StateComponent& state);
};
