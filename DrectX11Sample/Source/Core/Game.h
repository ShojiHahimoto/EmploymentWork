#pragma once

#include "Core/Renderer.h"
#include "Manager/LoadWorker.h"
#include "Manager/SaveWorker.h"

//------------------------------------------
// ゲーム本体クラス
//------------------------------------------
class Game
{
private:
	Renderer renderer;
	LoadWorker loadWorker;
	SaveWorker saveWorker;
	bool rendererInitialized = false;

public:
	void Init();
	void Update();
	void Draw();
	void OnResize(int width, int height);
	void Uninit();
};
