#pragma once

#include "Controller/BattleSetupControllers.h"
#include "Scene/Scene.h"
#include "World/World.h"

class BattleSetupScene : public Scene
{
public:
	BattleSetupScene(int initialWidth, int initialHeight);
	~BattleSetupScene() override = default;

	void Enter() override;
	void Exit() override;
	void RunSystems() override;
	void Draw(Renderer& renderer) override;
	void OnResize(int newWidth, int newHeight) override;

	World& GetWorld() override;
	const World& GetWorld() const override;

private:
	World world;
	BattleSetupSession session;
	BattleSetupFlowController flowController;
};
