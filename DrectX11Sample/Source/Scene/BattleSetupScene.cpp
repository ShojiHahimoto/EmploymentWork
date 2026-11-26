#include "Scene/BattleSetupScene.h"

#include "Manager/SceneLoadRequest.h"

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
	flowController.Enter(saveWorker);
}

/// <summary>対戦設定のキャラクターと入力設定一覧を遷移前に準備する。</summary>
/// <param name="request">ワーカーへ渡すロード要求。</param>
void BattleSetupScene::DescribeLoad(SceneLoadRequest &request) const
{
	request.uiImageIds = {"header_accent", "back", "panel", "battle_panel", "button", "button_primary",
		"badge_a", "badge_b", "badge_x", "badge_y", "battle_keyboard", "battle_gamepad",
		"battle_tab_left", "battle_tab_right", "battle_surface", "battle_roster_button"};
	request.prepareScene = []
	{
		BattleSetupSession prepared;
		BattleSetupCharacterSelectionController(prepared).RefreshCharacterSlotSummaries();
		BattleSetupInputConfigController(prepared).RefreshInputConfigSummaries();

		PreparedSceneLoad result;
		result.apply = [prepared = std::move(prepared)](Scene &scene) mutable
		{
			auto &session = static_cast<BattleSetupScene &>(scene).session;
			session.characterSlotSummaries = std::move(prepared.characterSlotSummaries);
			session.keyboardInputConfigs = std::move(prepared.keyboardInputConfigs);
			session.gamepadInputConfigs = std::move(prepared.gamepadInputConfigs);
		};

		return result;
	};
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
void BattleSetupScene::Update()
{
	flowController.Update();
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
