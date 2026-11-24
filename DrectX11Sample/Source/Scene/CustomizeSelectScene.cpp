#include "Scene/CustomizeSelectScene.h"

CustomizeSelectScene::CustomizeSelectScene(int initialWidth, int initialHeight, CustomizeMode initialMode,
	CustomizeAttackCategory category)
	: flowController(initialWidth, initialHeight, initialMode, category)
{
}

void CustomizeSelectScene::Enter()
{
	flowController.Enter();
}

void CustomizeSelectScene::Exit()
{
	world.Clear();
}

void CustomizeSelectScene::Update()
{
	flowController.Update();
}

void CustomizeSelectScene::Draw(Renderer &renderer)
{
	flowController.DrawInterface(renderer);
}

void CustomizeSelectScene::OnResize(int newWidth, int newHeight)
{
	flowController.OnResize(newWidth, newHeight);
}

World &CustomizeSelectScene::GetWorld()
{
	return world;
}

const World &CustomizeSelectScene::GetWorld() const
{
	return world;
}
