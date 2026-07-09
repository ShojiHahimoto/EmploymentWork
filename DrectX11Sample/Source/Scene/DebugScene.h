#pragma once

#include "Scene/Scene.h"
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
	int width;
	int height;
};
