#include "Scene/SceneManager.h"

/// <summary>
/// SceneManager の唯一のインスタンスを取得する。
/// </summary>
/// <returns>SceneManager の参照。</returns>
SceneManager& SceneManager::GetInstance()
{
	static SceneManager instance;
	return instance;
}

/// <summary>
/// 次フレーム境界で切り替える Scene を予約する。
/// </summary>
/// <param name="nextScene">次に有効化する Scene。</param>
void SceneManager::RequestChangeScene(std::unique_ptr<Scene> nextScene)
{
	pendingScene = std::move(nextScene);
}

/// <summary>
/// 予約されている Scene 切り替えを適用し、旧 Scene の終了処理と新 Scene の開始処理を行う。
/// </summary>
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

/// <summary>
/// 現在有効な Scene を取得する。
/// </summary>
/// <returns>現在の Scene。存在しない場合は nullptr。</returns>
Scene* SceneManager::GetCurrentScene()
{
	return currentScene.get();
}

/// <summary>
/// 現在有効な Scene を読み取り専用で取得する。
/// </summary>
/// <returns>現在の Scene。存在しない場合は nullptr。</returns>
const Scene* SceneManager::GetCurrentScene() const
{
	return currentScene.get();
}

/// <summary>
/// 現在有効な Scene が存在するか確認する。
/// </summary>
/// <returns>Scene が存在すれば true。</returns>
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

/// <summary>
/// 現在の Scene の描画処理を呼び出す。
/// </summary>
/// <param name="renderer">Scene 描画に使う Renderer。</param>
void SceneManager::Draw(Renderer& renderer)
{
	if (currentScene)
	{
		currentScene->Draw(renderer);
	}
}

/// <summary>
/// ウィンドウサイズ変更を現在の Scene に通知する。
/// </summary>
/// <param name="width">新しい幅。</param>
/// <param name="height">新しい高さ。</param>
void SceneManager::OnResize(int width, int height)
{
	if (currentScene)
	{
		currentScene->OnResize(width, height);
	}
}

/// <summary>
/// 予約 Scene と現在 Scene を破棄し、現在 Scene があれば終了処理を呼ぶ。
/// </summary>
void SceneManager::Shutdown()
{
	pendingScene.reset();

	if (currentScene)
	{
		currentScene->Exit();
		currentScene.reset();
	}
}
