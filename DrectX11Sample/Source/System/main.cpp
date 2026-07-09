#include "System/main.h"
#include "System/Application.h"

//=================================
// エントリーポイント
//=================================
int main(void)
{
	//メモリリーク検出機能、デバッグモードのみ有効
#if defined(DEBUG) || defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	//アプリケーション実行
	Application app(SCREEN_WIDTH, SCREEN_HEIGHT);
	app.Run();

	return 0;
}
