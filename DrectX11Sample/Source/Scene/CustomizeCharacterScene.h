#pragma once

#include "Controller/CustomizeCharacterFlowController.h"
#include "Core/World.h"
#include "Scene/Scene.h"

class CustomizeCharacterScene : public Scene
{
  public:
	CustomizeCharacterScene(int initialWidth, int initialHeight, int slotIndex);

	void Enter() override;
	void DescribeLoad(SceneLoadRequest &request) const override;
	void Exit() override;
	void Update() override;
	void Draw(Renderer &renderer) override;
	void OnResize(int width, int height) override;

	World &GetWorld() override;
	const World &GetWorld() const override;

  private:
	World world;
	CustomizeCharacterFlowController flow;
	int slotIndex;
};
