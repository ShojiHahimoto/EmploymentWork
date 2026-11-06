#include "Scene/CustomizeScene.h"

CustomizeScene::CustomizeScene(int initialWidth, int initialHeight)
	: flowController(world, initialWidth, initialHeight)
{
}
void CustomizeScene::Enter()
{
	flowController.Enter();
}

void CustomizeScene::Exit()
{
	flowController.Exit();
	world.Clear();
}

void CustomizeScene::RunSystems()
{
	flowController.RunSystems();
}

void CustomizeScene::Draw(Renderer& renderer)
{
	flowController.Draw(renderer);
}

void CustomizeScene::OnResize(int newWidth, int newHeight)
{
	flowController.OnResize(newWidth, newHeight);
}

World& CustomizeScene::GetWorld()
{
	return world;
}

const World& CustomizeScene::GetWorld() const
{
	return world;
}
