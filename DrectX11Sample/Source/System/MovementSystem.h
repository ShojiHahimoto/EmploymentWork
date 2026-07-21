#pragma once

#include <SimpleMath.h>

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

private:
	static void ApplyVelocityToTransform(World& world);
};
