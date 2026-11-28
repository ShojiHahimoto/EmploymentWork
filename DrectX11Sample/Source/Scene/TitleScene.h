#pragma once

#include "Controller/TitleMenuController.h"
#include "Controller/TitleNavigationController.h"
#include "Controller/TitlePresentationController.h"
#include "Scene/Scene.h"
#include "Core/World.h"

class TitleScene : public Scene
{
public:
	TitleScene(int initialWidth, int initialHeight);
	~TitleScene() override = default;

	void Enter() override;
	void DescribeLoad(SceneLoadRequest &request) const override;
	void Exit() override;
	void Update() override;
	void Draw(Renderer& renderer) override;
	void OnResize(int newWidth, int newHeight) override;

	World& GetWorld() override;
	const World& GetWorld() const override;

private:
	World world;
	TitlePresentationController presentationController;
	PreparedTitlePresentation preparedPresentation;
	int width = 1;
	int height = 1;
	TitleMenuController menuController;
	TitleNavigationController navigationController;
};
