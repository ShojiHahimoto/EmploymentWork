#include "Scene/CustomizeAttackScene.h"

#include <utility>

CustomizeAttackScene::CustomizeAttackScene(int initialWidth, int initialHeight, CustomizeAttackTarget target)
	: flowController(initialWidth, initialHeight, std::move(target))
{
}

void CustomizeAttackScene::Enter()
{
	flowController.Enter();
}

void CustomizeAttackScene::Exit()
{
	flowController.Exit();
	world.Clear();
}

void CustomizeAttackScene::Update()
{
	flowController.Update();
}

void CustomizeAttackScene::Draw(Renderer &renderer)
{
	flowController.DrawBackdrop(renderer);
	flowController.DrawPreview(renderer);
	flowController.DrawInterface(renderer);
}

void CustomizeAttackScene::OnResize(int newWidth, int newHeight)
{
	flowController.OnResize(newWidth, newHeight);
}

World &CustomizeAttackScene::GetWorld()
{
	return world;
}

const World &CustomizeAttackScene::GetWorld() const
{
	return world;
}
