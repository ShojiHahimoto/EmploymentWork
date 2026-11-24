#pragma once

#include "Controller/CustomizeAttackFlowController.h"
#include "Scene/Scene.h"
#include "Core/World.h"

class CustomizeAttackScene : public Scene
{
  public:
	CustomizeAttackScene(int initialWidth, int initialHeight, CustomizeAttackTarget target);
	~CustomizeAttackScene() override = default;

	void Enter() override;
	void Exit() override;
	void Update() override;
	void Draw(Renderer &renderer) override;
	void OnResize(int newWidth, int newHeight) override;

	World &GetWorld() override;
	const World &GetWorld() const override;

  private:
	World world;
	CustomizeAttackFlowController flowController;
};
