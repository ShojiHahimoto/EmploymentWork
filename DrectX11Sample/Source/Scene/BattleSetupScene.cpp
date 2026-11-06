#include "Scene/BattleSetupScene.h"

/// <summary>
/// セットアップ用データと画面フロー担当コントローラーを初期化する。
/// </summary>
/// <param name="initialWidth">初期ウィンドウ幅。</param>
/// <param name="initialHeight">初期ウィンドウ高さ。</param>
BattleSetupScene::BattleSetupScene(int initialWidth, int initialHeight)
	: flowController(session, initialWidth, initialHeight)
{
}

/// <summary>
/// セットアップ画面の開始処理をControllerへ委譲する。
/// </summary>
void BattleSetupScene::Enter()
{
	flowController.Enter();
}

/// <summary>
/// Controllerを終了し、Sceneが所有するWorldを解放する。
/// </summary>
void BattleSetupScene::Exit()
{
	flowController.Exit();
	world.Clear();
}

/// <summary>
/// セットアップ処理の更新をControllerへ委譲する。
/// </summary>
void BattleSetupScene::RunSystems()
{
	flowController.RunSystems();
}

/// <summary>
/// セットアップ画面の描画をControllerへ委譲する。
/// </summary>
/// <param name="renderer">Scene描画に使う Renderer。</param>
void BattleSetupScene::Draw(Renderer& renderer)
{
	flowController.Draw(renderer);
}

/// <summary>
/// 新しい画面サイズをControllerへ通知する。
/// </summary>
/// <param name="newWidth">新しい幅。</param>
/// <param name="newHeight">新しい高さ。</param>
void BattleSetupScene::OnResize(int newWidth, int newHeight)
{
	flowController.OnResize(newWidth, newHeight);
}

/// <summary>
/// Scene が所有する World を取得する。
/// </summary>
/// <returns>変更可能な World。</returns>
World& BattleSetupScene::GetWorld()
{
	return world;
}

/// <summary>
/// Scene が所有する World を読み取り専用で取得する。
/// </summary>
/// <returns>読み取り専用の World。</returns>
const World& BattleSetupScene::GetWorld() const
{
	return world;
}
