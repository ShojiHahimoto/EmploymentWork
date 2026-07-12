#include "System/Game.h"
#include "Scene/BattleScene.h"
#include "Scene/SceneManager.h"
#include "System/Application.h"
#include "System/DebugImGuiSystem.h"

#include <memory>

void Game::Init()
{
	rendererInitialized = SUCCEEDED(renderer.Init());
	if (!rendererInitialized)
	{
		return;
	}

	DebugImGuiSystem::Init(
		Application::GetWindow(),
		Renderer::GetDevice(),
		Renderer::GetDeviceContext());

	SceneManager& sceneManager = SceneManager::GetInstance();
	sceneManager.RequestChangeScene(
		std::make_unique<BattleScene>(
			static_cast<int>(Application::GetWidth()),
			static_cast<int>(Application::GetHeight())));
	sceneManager.ApplyPendingSceneChange();
}

void Game::Update()
{
	SceneManager& sceneManager = SceneManager::GetInstance();
	sceneManager.RunSystems();
	sceneManager.ApplyPendingSceneChange();
}

void Game::Draw()
{
	if (!rendererInitialized)
	{
		return;
	}

	renderer.DrawStart();
	DebugImGuiSystem::BeginFrame();
	SceneManager::GetInstance().Draw(renderer);
	DebugImGuiSystem::Render();
	renderer.DrawEnd();
}

void Game::OnResize(int width, int height)
{
	if (!rendererInitialized || width <= 0 || height <= 0)
	{
		return;
	}

	if (FAILED(Renderer::ResizeWindow(width, height)))
	{
		rendererInitialized = false;
		return;
	}

	SceneManager::GetInstance().OnResize(width, height);
}

void Game::Uninit()
{
	SceneManager::GetInstance().Shutdown();
	DebugImGuiSystem::Shutdown();
	rendererInitialized = false;

	renderer.Uninit();
}
