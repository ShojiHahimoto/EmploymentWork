#include "Scene/TitleScene.h"

/// <summary>
/// World とタイトル画面の担当コントローラーを初期化する。
/// </summary>
/// <param name="initialWidth">初期ウィンドウ幅。</param>
/// <param name="initialHeight">初期ウィンドウ高さ。</param>
TitleScene::TitleScene(int initialWidth, int initialHeight)
	: menuController(initialWidth, initialHeight)
{
}

/// <summary>
/// タイトル用の入力マップを有効にする。
/// </summary>
void TitleScene::Enter()
{
	navigationController.Enter();
}

/// <summary>
/// タイトル Scene の World を破棄して終了する。
/// </summary>
void TitleScene::Exit()
{
	world.Clear();
}

/// <summary>
/// 遷移Controllerを更新し、メニューからの要求を渡す。
/// </summary>
void TitleScene::RunSystems()
{
	navigationController.RunSystems(menuController.ConsumeCommand());
}

/// <summary>
/// タイトルメニューControllerへ描画を委譲する。
/// </summary>
/// <param name="renderer">Scene描画に使う Renderer。</param>
void TitleScene::Draw(Renderer& renderer)
{
	(void)renderer;
	menuController.Draw();
}

/// <summary>
/// メニュー配置Controllerへ新しい画面サイズを通知する。
/// </summary>
/// <param name="newWidth">新しい幅。</param>
/// <param name="newHeight">新しい高さ。</param>
void TitleScene::OnResize(int newWidth, int newHeight)
{
	menuController.OnResize(newWidth, newHeight);
}

/// <summary>
/// Scene が所有する World を取得する。
/// </summary>
/// <returns>変更可能な World。</returns>
World& TitleScene::GetWorld()
{
	return world;
}

/// <summary>
/// Scene が所有する World を読み取り専用で取得する。
/// </summary>
/// <returns>読み取り専用の World。</returns>
const World& TitleScene::GetWorld() const
{
	return world;
}
