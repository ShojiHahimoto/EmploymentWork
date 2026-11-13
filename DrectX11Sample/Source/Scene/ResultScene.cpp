#include "Scene/ResultScene.h"

/// <summary>
/// World と結果表示コントローラーをバトル結果で初期化する。
/// </summary>
/// <param name="initialWidth">初期ウィンドウ幅。</param>
/// <param name="initialHeight">初期ウィンドウ高さ。</param>
/// <param name="result">BattleScene が確定した勝敗結果。</param>
ResultScene::ResultScene(int initialWidth, int initialHeight, BattleResult result)
	: presentationController(result, initialWidth, initialHeight)
	, navigationController(result)
{
}

/// <summary>
/// リザルト入力と遷移Controllerを開始する。
/// </summary>
void ResultScene::Enter()
{
	navigationController.Enter();
}

/// <summary>
/// Scene が所有する World を破棄して終了する。
/// </summary>
void ResultScene::Exit()
{
	world.Clear();
}

/// <summary>
/// 遷移Controllerへ表示側のボタン要求を渡して更新する。
/// </summary>
void ResultScene::RunSystems()
{
	navigationController.RunSystems(presentationController.ConsumeReturnRequest());
}

/// <summary>
/// 結果表示Controllerへ描画を委譲する。
/// </summary>
/// <param name="renderer">Scene描画に使う Renderer。</param>
void ResultScene::Draw(Renderer& renderer)
{
	(void)renderer;
	presentationController.Draw();
}

/// <summary>
/// 結果表示Controllerへ新しい画面サイズを通知する。
/// </summary>
/// <param name="newWidth">新しい幅。</param>
/// <param name="newHeight">新しい高さ。</param>
void ResultScene::OnResize(int newWidth, int newHeight)
{
	presentationController.OnResize(newWidth, newHeight);
}

/// <summary>
/// Scene が所有する World を取得する。
/// </summary>
/// <returns>変更可能な World。</returns>
World& ResultScene::GetWorld()
{
	return world;
}

/// <summary>
/// Scene が所有する World を読み取り専用で取得する。
/// </summary>
/// <returns>読み取り専用の World。</returns>
const World& ResultScene::GetWorld() const
{
	return world;
}
