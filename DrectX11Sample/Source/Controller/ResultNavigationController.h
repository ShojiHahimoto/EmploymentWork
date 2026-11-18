#pragma once

#include "World/World.h"

class ResultNavigationController
{
public:
	explicit ResultNavigationController(BattleResult result);
	void Enter();
	void Update(bool returnRequestedByView);

private:
	BattleResult result;

	bool WasSubmitTriggered() const;
	void RequestTitleScene();
};
