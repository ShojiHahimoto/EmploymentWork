#pragma once

#include "Scene/Scene.h"
#include "World/World.h"

#include <d3d11.h>

class TitleScene : public Scene
{
public:
	TitleScene(int initialWidth, int initialHeight);
	~TitleScene() override = default;

	void Enter() override;
	void Exit() override;
	void RunSystems() override;
	void Draw(Renderer& renderer) override;
	void OnResize(int newWidth, int newHeight) override;

	World& GetWorld() override;
	const World& GetWorld() const override;

private:
	World world;
	ID3D11ShaderResourceView* backgroundTexture = nullptr;
	int width = 0;
	int height = 0;
	bool customizeKeyPressedLastFrame = false;

	bool WasSubmitTriggered() const;
	bool WasCustomizeTriggered();
};
