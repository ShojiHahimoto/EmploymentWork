#include "Scene/CustomizeSelectScene.h"

#include "Manager/SceneLoadRequest.h"

CustomizeSelectScene::CustomizeSelectScene(int initialWidth, int initialHeight, CustomizeMode initialMode,
	CustomizeAttackCategory category)
	: flowController(initialWidth, initialHeight, initialMode, category)
{
}

/// <summary>選択メニューが使用する画像を要求する。</summary>
/// <param name="request">シーン入場前のロード要求。</param>
void CustomizeSelectScene::DescribeLoad(SceneLoadRequest &request) const
{
	request.bgmId = "BGM/BGM_Customize_01_maou";
	request.uiImageIds = {"customize_menu_normal", "customize_menu_selected", "header", "header_accent",
						  "button", "back", "panel", "slot_focused", "slot_forward",
						  "badge_a", "badge_b", "badge_x", "badge_y"};
}

/// <summary>共通ロードワーカーを選択画面へ渡し、入場時の一覧読み込みに使えるようにする。</summary>
void CustomizeSelectScene::Enter()
{
	flowController.Enter(loadWorker, saveWorker);
}

void CustomizeSelectScene::Exit()
{
	world.Clear();
}

void CustomizeSelectScene::Update()
{
	flowController.Update();
}

void CustomizeSelectScene::Draw(Renderer &renderer)
{
	flowController.DrawInterface(renderer);
}

void CustomizeSelectScene::OnResize(int newWidth, int newHeight)
{
	flowController.OnResize(newWidth, newHeight);
}

World &CustomizeSelectScene::GetWorld()
{
	return world;
}

const World &CustomizeSelectScene::GetWorld() const
{
	return world;
}
