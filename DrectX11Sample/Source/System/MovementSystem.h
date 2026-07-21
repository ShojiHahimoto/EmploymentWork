#pragma once

class World;

class MovementSystem
{
public:
	// 入力から Velocity を決め、Velocity を Transform に反映する。
	// 現段階では VelocityComponent を持つオブジェクトを操作対象にする。
	static void Update(World& world);

private:
	// 固定 60fps 前提の 1 フレームあたり移動量。
	static constexpr float MoveSpeedPerFrame = 0.08f;

	static void UpdateVelocityFromInput(World& world);
	static void ApplyVelocityToTransform(World& world);
};
