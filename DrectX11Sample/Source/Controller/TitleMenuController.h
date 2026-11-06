#pragma once

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
	void Draw();
	void OnResize(int width, int height);
	TitleMenuCommand ConsumeCommand();

private:
	TitleMenuCommand pendingCommand = TitleMenuCommand::None;
	int width = 0;
	int height = 0;
};
