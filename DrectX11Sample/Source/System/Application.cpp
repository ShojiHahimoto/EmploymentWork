#include "System/Application.h"
#include "System/Game.h"
#include "System/DebugImGuiSystem.h"
#include <chrono>
#include <thread>

//Imgui

const auto ClassName = TEXT("2025 framework ひな型");	//ウィンドウクラス名
const auto WindowName = TEXT("2025 framework ひな型");	//ウィンドウ名

HINSTANCE	Application::m_hInst;		//インスタンスハンドル
HWND		Application::m_hWnd;		//ウィンドウハンドル
uint32_t	Application::m_Width;		//ウィンドウの横幅
uint32_t	Application::m_Height;		//ウィンドウの縦幅

//ゲームインスタンス
static Game game;

//---------------------------------------------------------------------------
// コンストラクタ
//---------------------------------------------------------------------------
Application::Application(uint32_t width, uint32_t height)
{
	m_Height = height;
	m_Width = width;

	timeBeginPeriod(1);	//タイマーの制度を1ミリ秒に設定
}

//---------------------------------------------------------------------------
// デストラクタ
//---------------------------------------------------------------------------
Application::~Application()
{
	timeEndPeriod(1);	//タイマー制度を元に戻す
}

//---------------------------------------------------------------------------
// 実行
//---------------------------------------------------------------------------
void Application::Run()
{
	//初期化
	bool okfg = InitApp();
	if (okfg) { MainLoop(); }

	UninitApp();	//終了処理
}

//---------------------------------------------------------------------------
// 初期化処理
//---------------------------------------------------------------------------
bool Application::InitApp()
{
	//インスタンスハンドルを取得
	auto hInst = GetModuleHandle(nullptr);
	if (hInst == nullptr)
	{
		return false;
	}

	//ウィンドウの設定
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hIcon = LoadIcon(hInst, IDI_APPLICATION);
	wc.hCursor = LoadCursor(hInst, IDC_ARROW);
	wc.hbrBackground = GetSysColorBrush(COLOR_BACKGROUND);
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = ClassName;
	wc.hIconSm = LoadIcon(hInst, IDI_APPLICATION);

	//ウィンドウの登録
	if (!RegisterClassEx(&wc))
	{
		return false;
	}

	//インスタンスハンドル設定
	m_hInst = hInst;

	//ウインドウのサイズを設定
	RECT rc = {};
	rc.right = static_cast<LONG>(m_Width);
	rc.bottom = static_cast<LONG>(m_Height);

	//ウィンドウサイズを調整
	auto style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
	AdjustWindowRect(&rc, style, FALSE);

	//ウィンドウを生成
	m_hWnd = CreateWindowEx(
		0,
		//WS_EX_TOPMOST,
		ClassName,
		WindowName,
		style,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rc.right - rc.left,
		rc.bottom - rc.top,
		nullptr,
		nullptr,
		m_hInst,
		nullptr);

	if (m_hWnd == nullptr)
	{
		return false;
	}

	//ウィンドウを表示
	ShowWindow(m_hWnd, SW_SHOWNORMAL);

	//ウィンドウを更新
	UpdateWindow(m_hWnd);

	//ウィンドウにフォーカスを設定
	SetFocus(m_hWnd);

	//ゲーム初期化
	game.Init();

	//正常終了
	return true;
}

//---------------------------------------------------------------------------
// 終了処理
//---------------------------------------------------------------------------
void Application::UninitApp()
{
	//ゲーム終了
	game.Uninit();

	//ウィンドウの登録を解除
	if (m_hInst != nullptr)
	{
		UnregisterClass(ClassName, m_hInst);
	}

	m_hInst = nullptr;
	m_hWnd = nullptr;
}

//---------------------------------------------------------------------------
// メインループ
//---------------------------------------------------------------------------
void Application::MainLoop()
{
	MSG msg = {};

	//描画初期化処理

	//ゲームオブジェクト

	//FPS計測用変数
	int fpsCounter = 0;
	long long oldTick = GetTickCount64();	//前回計測時の時間
	long long nowTick = oldTick;	//今回計測時の時間

	//FPS固定用変数
	LARGE_INTEGER liWork;	//workがつく変数は作業用変数
	long long frequency;	//どれくらい細かく時間をカウントできるか
	QueryPerformanceFrequency(&liWork);
	frequency = liWork.QuadPart;
	//時間（単位：カウント）取得
	QueryPerformanceCounter(&liWork);
	long long oldCount = liWork.QuadPart;	//前回計測時の時間
	long long nowCount = oldCount;	//今回計測時の時間

	//描画制御用タイマー
	long long renderCount = oldCount;

	//フレーム時間
	const long long frameStep = frequency / 60;

	//ActiveFullScreen(true);		//フルスクリーンにする

	//ゲームループ
	while (1)
	{
		//新たにメッセージがあれば
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			//ウィンドウプロシージャにメッセージを送る
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			//「WM_QUIT」メッセージを受け取ったらループを抜ける
			if (msg.message == WM_QUIT) {
				break;
			}
		}
		else
		{
			QueryPerformanceCounter(&liWork);	//現在時間を取得
			nowCount = liWork.QuadPart;
			//--------------------------------------------------
			// Update（フレーム完全固定）
			//--------------------------------------------------
			while (nowCount >= oldCount + frameStep)
			{
				//アップデード処理
				game.Update();

				fpsCounter++;	//ゲーム処理を実行したら＋１する
				//oldCount = nowCount;
				oldCount += frequency / 60;
			}
			//--------------------------------------------------
			// Draw（最大60fps制限）
			//--------------------------------------------------
			if (nowCount >= renderCount + frameStep)
			{
				game.Draw();

				//描画タイミング更新
				renderCount += frameStep;
			}
			//--------------------------------------------------
			// CPU負荷軽減
			//--------------------------------------------------
			Sleep(1);
		}
	}

	//シーン処理終了
	//描画処理終了
}

void Application::Destroy()
{
	DestroyWindow(GetWindow());
}

//---------------------------------------------------------------------------
// メインループ
//---------------------------------------------------------------------------
LRESULT CALLBACK Application::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (DebugImGuiSystem::HandleWndProc(hWnd, uMsg, wParam, lParam))
	{
		return true;
	}
	
	static bool isFullscreen = false;
	static bool isMessageBoxShowed = false;

	switch (uMsg)
	{
	case WM_DESTROY:	//ウィンドウ破棄のメッセージ
		PostQuitMessage(0);	//「WM_QUIT」メッセージを送る　→　アプリ終了
		break;

	case WM_CLOSE:	//「×」ボタンが押されたら
	{
		//int res = MessageBoxA(NULL, "終了しますか？", "確認", MB_OKCANCEL);
		//if (res == IDOK) 
		{
			DestroyWindow(hWnd);	//「WM_DESTROY」メッセージを送る
		}
	}
	break;

	case WM_KEYDOWN:	//キー入力があったメッセージ
		if (LOWORD(wParam) == VK_ESCAPE)	//入力されたキーがEscapeなら
		{
			//PostMessage(hWnd, WM_CLOSE, wParam, lParam);	//「WM_CLOSE」を送る
		}

		else if (LOWORD(wParam) == VK_F11)
		{
			isFullscreen = !isFullscreen;

			ActiveFullScreen(isFullscreen);
		}
		break;

	case WM_ACTIVATE:
		if (wParam == WA_INACTIVE) {
			//フルスクリーン表示かつメッセージボックスが非表示なら
			if (isFullscreen && !isMessageBoxShowed)
			{
				//ウィンドウを最小化する（タスク切り替え時に背後に残る問題対策）
				ShowWindow(hWnd, SW_MINIMIZE);
			}
		}
		//標準挙動を実行
		return DefWindowProc(hWnd, uMsg, wParam, lParam);

	case WM_SIZE:	//ウィンドウサイズに変更があったメッセージ
		if (wParam != SIZE_MINIMIZED)
		{
			int width = LOWORD(lParam);	//横幅
			int height = HIWORD(lParam);	//縦幅
			game.OnResize(width, height);
			//レンダラークラスリサイズ処理
		}
		break;

	default:
		//受け取ったメッセージに対してデフォルトの処理を実行
		return DefWindowProcW(hWnd, uMsg, wParam, lParam);
		break;
	}
	return 0;
}

//---------------------------------------------------------------------------
// フルスクリーン切り替え処理
//---------------------------------------------------------------------------
void Application::ActiveFullScreen(bool _isFullScreen)
{
	if (_isFullScreen)
	{
		//疑似フルスクリーンモードに変更
		SetWindowLongPtr(Application::GetWindow(), GWL_STYLE, WS_POPUP | WS_MINIMIZEBOX);	//ウィンドウ枠を削除
		//ディスプレイ解像度を取得
		int screenWidth = GetSystemMetrics(SM_CXSCREEN);
		int screenHeight = GetSystemMetrics(SM_CYSCREEN);
		SetWindowPos(Application::GetWindow(), HWND_TOP, 0, 0, screenWidth, screenHeight, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
	}
	else
	{
		//通常ウィンドウに戻す
		SetWindowLongPtr(Application::GetWindow(), GWL_STYLE, WS_OVERLAPPEDWINDOW);	//ウィンドウ枠を戻す
		SetWindowPos(Application::GetWindow(), HWND_TOP, 100, 100, m_Width, m_Height, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
	}
}
