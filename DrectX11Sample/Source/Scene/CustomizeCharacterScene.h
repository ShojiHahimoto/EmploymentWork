#pragma once

#include "Controller/CustomizeCharacterEditorController.h"
#include "Core/World.h"
#include "Scene/Scene.h"

#include <string>

class CustomizeCharacterScene : public Scene
{
  public:
	CustomizeCharacterScene(int initialWidth, int initialHeight, int slotIndex);

	void Enter() override;
	void Exit() override;
	void Update() override;
	void Draw(Renderer &renderer) override;
	void OnResize(int width, int height) override;

	World &GetWorld() override;
	const World &GetWorld() const override;

  private:
	World world;
	CustomizeCharacterEditorController editor;
	CustomizeMode mode = CustomizeMode::CharacterEditor;
	std::string statusMessage;
	int width;
	int height;
	int slotIndex;

	void RequestSlotSelection();
};
