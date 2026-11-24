#pragma once

#include "Core/GameObject.h"

#include <SimpleMath.h>

class World;
struct HitReactionRequest;
enum class CameraYFollowMode;
enum class DownMotionType;

class HitReactionSystem
{
public:
	static void Update(World& world);

private:
	static void ApplyReactionRequest(World& world, const HitReactionRequest& request);
	static void ApplyNormalBack(World& world, const HitReactionRequest& request, float backDistance);
	static void ApplyCornerAttackerBack(World& world, const HitReactionRequest& request, float backDistance);
	static void ApplyDown(World& world, GameObjectId defenderId, int downFrames, DownMotionType downMotionType);
	static void ApplyAirBurst(
		World& world,
		const HitReactionRequest& request,
		const DirectX::SimpleMath::Vector3& baseVelocity,
		CameraYFollowMode cameraYFollowMode);
	static void ResolveLandedAirHitstun(World& world);
};
