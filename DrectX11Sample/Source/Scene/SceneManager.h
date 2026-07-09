#pragma once

#include "Scene/Scene.h"
#include <memory>

class SceneManager
{
public:
	static SceneManager& GetInstance();

	void RequestChangeScene(std::unique_ptr<Scene> nextScene);
	void ApplyPendingSceneChange();

	Scene* GetCurrentScene();
	const Scene* GetCurrentScene() const;
	bool HasScene() const;

	void RunSystems();
	void Draw(Renderer& renderer);
	void OnResize(int width, int height);
	void Shutdown();

private:
	SceneManager() = default;
	~SceneManager() = default;

	SceneManager(const SceneManager&) = delete;
	SceneManager& operator=(const SceneManager&) = delete;

	std::unique_ptr<Scene> currentScene;
	std::unique_ptr<Scene> pendingScene;
};
