#pragma once

#include "Controller/TitleMenuController.h"

class TitleNavigationController
{
public:
	void Enter();
	void Update(TitleMenuCommand menuCommand);

private:
	bool customizeKeyPressedLastFrame = false;
	bool battleShortcutKeyPressedLastFrame = false;

	bool WasCustomizeTriggered();
	bool WasBattleShortcutTriggered();
	void RequestBattleSetupScene();
	void RequestCustomizeScene();
	void RequestBattleScene();
};
