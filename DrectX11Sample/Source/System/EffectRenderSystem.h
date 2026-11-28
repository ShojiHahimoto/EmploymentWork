#pragma once

struct CameraComponent;
class World;

class EffectRenderSystem
{
public:
	static void Draw(World& world, const CameraComponent& camera);

	static void ReleaseResources();
};
