#pragma once

#include "Controller/ResultNavigationController.h"
#include "Controller/ResultPresentationController.h"
#include "Scene/Scene.h"
#include "Core/World.h"

class ResultScene : public Scene
{
public:
	ResultScene(int initialWidth, int initialHeight, BattleResult result);
	~ResultScene() override = default;

	void Enter() override;
	void Exit() override;
	void Update() override;
	void Draw(Renderer& renderer) override;
	void OnResize(int newWidth, int newHeight) override;

	World& GetWorld() override;
	const World& GetWorld() const override;

private:
	World world;
	ResultPresentationController presentationController;
	ResultNavigationController navigationController;
};
