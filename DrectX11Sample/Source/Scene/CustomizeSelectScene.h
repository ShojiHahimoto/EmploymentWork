#pragma once

#include "Controller/CustomizeFlowController.h"
#include "Scene/Scene.h"
#include "Core/World.h"

class CustomizeSelectScene : public Scene
{
  public:
	CustomizeSelectScene(int initialWidth, int initialHeight,
		CustomizeMode initialMode = CustomizeMode::MainMenu,
		CustomizeAttackCategory category = CustomizeAttackCategory::Ground);
	~CustomizeSelectScene() override = default;

	void Enter() override;
	void DescribeLoad(SceneLoadRequest &request) const override;
	void Exit() override;
	void Update() override;
	void Draw(Renderer &renderer) override;
	void OnResize(int newWidth, int newHeight) override;

	World &GetWorld() override;
	const World &GetWorld() const override;

  private:
	World world;
	CustomizeFlowController flowController;
};
