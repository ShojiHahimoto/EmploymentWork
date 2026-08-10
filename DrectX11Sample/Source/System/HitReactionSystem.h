#pragma once

#include "Core/GameObject.h"

#include <SimpleMath.h>

class World;
struct HitReactionRequest;

class HitReactionSystem
{
public:
	/// <summary>
	/// HitResolveSystem が確定した被弾反応を、座標補正や吹き飛び Velocity へ変換する。
	/// </summary>
	/// <param name="world">被弾反応リクエストと対象 Component を保持する World。</param>
	static void Update(World& world);

private:
	static void ApplyReactionRequest(World& world, const HitReactionRequest& request);
	static void ApplyNormalBack(World& world, const HitReactionRequest& request, float backDistance);
	static void ApplyDown(World& world, GameObjectId defenderId, int downFrames);
	static void ApplyAirBurst(World& world, const HitReactionRequest& request, const DirectX::SimpleMath::Vector3& baseVelocity);
	static void ResolveLandedAirHitstun(World& world);
};
