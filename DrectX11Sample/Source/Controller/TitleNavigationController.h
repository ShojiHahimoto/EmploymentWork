#pragma once

#include "Controller/TitleMenuController.h"

class TitleNavigationController
{
public:
	void Enter();
	void RunSystems(TitleMenuCommand menuCommand);

private:
	bool customizeKeyPressedLastFrame = false;
	bool battleShortcutKeyPressedLastFrame = false;

	bool WasSubmitTriggered() const;
	bool WasCustomizeTriggered();
	bool WasBattleShortcutTriggered();
	void RequestBattleSetupScene();
	void RequestCustomizeScene();
	void RequestBattleScene();
};
