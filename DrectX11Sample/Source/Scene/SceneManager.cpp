#include "Scene/SceneManager.h"

SceneManager& SceneManager::GetInstance()
{
	static SceneManager instance;
	return instance;
}

void SceneManager::RequestChangeScene(std::unique_ptr<Scene> nextScene)
{
	pendingScene = std::move(nextScene);
}

void SceneManager::ApplyPendingSceneChange()
{
	if (!pendingScene)
	{
		return;
	}

	if (currentScene)
	{
		currentScene->Exit();
		currentScene.reset();
	}

	currentScene = std::move(pendingScene);
	currentScene->Enter();
}

Scene* SceneManager::GetCurrentScene()
{
	return currentScene.get();
}

const Scene* SceneManager::GetCurrentScene() const
{
	return currentScene.get();
}

bool SceneManager::HasScene() const
{
	return currentScene != nullptr;
}

void SceneManager::RunSystems()
{
	if (currentScene)
	{
		currentScene->RunSystems();
	}
}

void SceneManager::Draw(Renderer& renderer)
{
	if (currentScene)
	{
		currentScene->Draw(renderer);
	}
}

void SceneManager::OnResize(int width, int height)
{
	if (currentScene)
	{
		currentScene->OnResize(width, height);
	}
}

void SceneManager::Shutdown()
{
	pendingScene.reset();

	if (currentScene)
	{
		currentScene->Exit();
		currentScene.reset();
	}
}
