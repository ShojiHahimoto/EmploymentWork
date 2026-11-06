#pragma once

#include "Scene/Scene.h"
#include "World/World.h"

class ResultScene : public Scene
{
public:
	ResultScene(int initialWidth, int initialHeight, BattleResult result);
	~ResultScene() override = default;

	void Enter() override;
	void Exit() override;
	void RunSystems() override;
	void Draw(Renderer& renderer) override;
	void OnResize(int newWidth, int newHeight) override;

	World& GetWorld() override;
	const World& GetWorld() const override;

private:
	World world;
	BattleResult battleResult = BattleResult::None;
	int width = 0;
	int height = 0;

	bool WasSubmitTriggered() const;
};
