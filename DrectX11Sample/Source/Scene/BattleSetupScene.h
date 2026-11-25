#pragma once

#include "Controller/BattleSetupControllers.h"
#include "Scene/Scene.h"
#include "Core/World.h"

class BattleSetupScene : public Scene
{
public:
	BattleSetupScene(int initialWidth, int initialHeight);
	~BattleSetupScene() override = default;

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
	BattleSetupSession session;
	BattleSetupFlowController flowController;
};
