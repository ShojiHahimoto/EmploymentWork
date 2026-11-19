#pragma once

#include "Controller/CustomizeFlowController.h"
#include "Scene/Scene.h"
#include "Core/World.h"

class CustomizeScene : public Scene
{
  public:
	CustomizeScene(int initialWidth, int initialHeight);
	~CustomizeScene() override = default;

	void Enter() override;
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
