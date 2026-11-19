#pragma once

#include "Core/Renderer.h"

//------------------------------------------
// ゲーム本体クラス
//------------------------------------------
class Game
{
private:
	Renderer renderer;
	bool rendererInitialized = false;

public:
	void Init();
	void Update();
	void Draw();
	void OnResize(int width, int height);
	void Uninit();
};
