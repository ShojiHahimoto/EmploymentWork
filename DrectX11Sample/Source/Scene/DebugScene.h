#pragma once

#include "Scene/Scene.h"
#include "System/DebugCameraControlSystem.h"
#include "World/World.h"

class DebugScene : public Scene
{
public:
	DebugScene(int initialWidth, int initialHeight);
	~DebugScene() override = default;

	void Enter() override;
	void Exit() override;
	void RunSystems() override;
	void Draw(Renderer& renderer) override;
	void OnResize(int width, int height) override;

	World& GetWorld() override;
	const World& GetWorld() const override;

private:
	World world;
	DebugCameraControlState debugCameraControlState;
	int width;
	int height;
};
