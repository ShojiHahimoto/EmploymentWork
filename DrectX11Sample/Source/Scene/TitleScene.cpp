#include "Scene/TitleScene.h"

#include "Manager/SceneLoadRequest.h"
#include "Data/BattleSetupData.h"

/// <summary>
/// World とタイトル画面の担当コントローラーを初期化する。
/// </summary>
/// <param name="initialWidth">初期ウィンドウ幅。</param>
/// <param name="initialHeight">初期ウィンドウ高さ。</param>
TitleScene::TitleScene(int initialWidth, int initialHeight)
	: menuController(initialWidth, initialHeight)
{
	width = initialWidth;
	height = initialHeight;
}

/// <summary>
/// タイトル表示に使う共通プレイヤーモデルを要求する。
/// </summary>
/// <param name="request">遷移前に準備する素材一覧。</param>
void TitleScene::DescribeLoad(SceneLoadRequest &request) const
{
	request.uiImageIds = {"title_v2_logo", "title_v2_normal", "title_v2_selected"};
	request.models.push_back({"DebugPlayer", "assets/model/DebugPlayer/man.fbx", true});
	request.motionIds.emplace_back("Common/Idle");
	request.motionIds.emplace_back("Common/JumpLoop");
	request.characterFolders.push_back(BattleSetup::BuildCharacterFolderPath(0));
	request.prepareScene = []
	{
		PreparedSceneLoad result;
		auto presentation = TitlePresentationController::Prepare();
		for (const auto &attack : presentation.attacks)
		{
			result.motionIds.push_back(attack.motionDataId);
		}

		result.apply = [presentation = std::move(presentation)](Scene &scene) mutable
		{
			static_cast<TitleScene &>(scene).preparedPresentation = std::move(presentation);
		};

		return result;
	};
}

/// <summary>
/// タイトル用の入力マップを有効にする。
/// </summary>
void TitleScene::Enter()
{
	navigationController.Enter();
	menuController.Enter();
	presentationController.Enter(world, preparedPresentation,
								 FindSceneCharacter(BattleSetup::BuildCharacterFolderPath(0)));
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
void TitleScene::Update()
{
	menuController.Update();
	presentationController.Update(world);
	navigationController.Update(menuController.ConsumeCommand());
}

/// <summary>
/// タイトルメニューControllerへ描画を委譲する。
/// </summary>
/// <param name="renderer">Scene描画に使う Renderer。</param>
void TitleScene::Draw(Renderer& renderer)
{
	presentationController.Draw(world, renderer, width, height);
	menuController.Draw(renderer);
}

/// <summary>
/// メニュー配置Controllerへ新しい画面サイズを通知する。
/// </summary>
/// <param name="newWidth">新しい幅。</param>
/// <param name="newHeight">新しい高さ。</param>
void TitleScene::OnResize(int newWidth, int newHeight)
{
	if (newWidth > 0 && newHeight > 0) { width = newWidth; height = newHeight; }
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
