#pragma once

#include "Scene/Scene.h"
#include "World/World.h"

class ResultPresentationController
{
public:
	ResultPresentationController(BattleResult result, int initialWidth, int initialHeight);

	void Draw();
	void OnResize(int width, int height);
	bool ConsumeReturnRequest();

private:
	BattleResult result;
	bool returnRequested = false;
	int width = 0;
	int height = 0;
};
