#pragma once

#include "UI/UIRoot.h"
#include "UI/UIRenderer.h"

enum class TitleMenuCommand
{
	None,
	StartBattleSetup,
	OpenCustomize
};

class TitleMenuController
{
public:
	TitleMenuController(int initialWidth, int initialHeight);
	void Enter();
	void Update();
	/// <summary>確定済み入力からメニューを更新する。描画では入力を処理しない。</summary>
	/// <param name="input">共通UI入力。</param>
	/// <param name="horizontal">UI左右軸。</param>
	/// <param name="submitDown">決定の押下状態。</param>
	void UpdateInput(const GameUI::InputFrame& input, float horizontal, bool submitDown);
	void Draw(Renderer& renderer);
	void OnResize(int width, int height);
	TitleMenuCommand ConsumeCommand();

private:
	GameUI::Rect GetButtonBounds(int index) const;
	GameUI::Rect MapRect(const GameUI::Rect& reference) const;
	void BuildDrawCommands();
	GameUI::Root inputRoot; // マウスの押下捕捉は共通UIへ委託する。
	std::vector<GameUI::DrawCommand> drawCommands;
	int selectedIndex = 0; // マウスとパッドが共有する選択。
	int previousDirection = 0;
	bool submitArmed = false; // 入場後は一度離すまで決定を受け付けない。
	bool previousSubmitDown = false;
	bool pointerArmed = false;
	TitleMenuCommand pendingCommand = TitleMenuCommand::None;
	int width = 0;
	int height = 0;
};
