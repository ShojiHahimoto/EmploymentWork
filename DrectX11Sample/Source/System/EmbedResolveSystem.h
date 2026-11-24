#pragma once

class World;

class EmbedResolveSystem
{
public:
	static constexpr float StageMinX = -45.0f;
	static constexpr float StageMaxX = 45.0f;

	static void Update(World& world);

private:
	static void ResolveWallBounds(World& world);
	static void ResolveCameraViewBounds(World& world);
	static void ResolvePlayerPushBoxes(World& world);
	static void ResolveTemporaryGround(World& world);
};
