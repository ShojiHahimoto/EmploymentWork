#include "System/Game.h"
#include "Scene/DebugScene.h"
#include "Scene/SceneManager.h"
#include "System/Application.h"

#include <memory>

void Game::Init()
{
	rendererInitialized = SUCCEEDED(renderer.Init());
	if (!rendererInitialized)
	{
		return;
	}

	SceneManager& sceneManager = SceneManager::GetInstance();
	sceneManager.RequestChangeScene(
		std::make_unique<DebugScene>(
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
	SceneManager::GetInstance().Draw(renderer);
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
	rendererInitialized = false;

	renderer.Uninit();
}
