#include "System/Application.h"
#include <chrono>
#include <thread>

// ★追加
#include "Game.h"

const auto ClassName = TEXT("2025 framework ひな型");
const auto WindowName = TEXT("2025 framework ひな型");

HINSTANCE    Application::m_hInst;
HWND        Application::m_hWnd;
uint32_t    Application::m_Width;
uint32_t    Application::m_Height;

// ★追加（ゲームインスタンス）
static Game game;


//---------------------------------------------------------------------------
// コンストラクタ
//---------------------------------------------------------------------------
Application::Application(uint32_t width, uint32_t height)
{
    m_Height = height;
    m_Width = width;

    timeBeginPeriod(1);
}

//---------------------------------------------------------------------------
// デストラクタ
//---------------------------------------------------------------------------
Application::~Application()
{
    timeEndPeriod(1);
}

//---------------------------------------------------------------------------
// 実行
//---------------------------------------------------------------------------
void Application::Run()
{
    bool okfg = InitApp();
    if (okfg) { MainLoop(); }

    UninitApp();
}

//---------------------------------------------------------------------------
// 初期化処理
//---------------------------------------------------------------------------
bool Application::InitApp()
{
    auto hInst = GetModuleHandle(nullptr);
    if (hInst == nullptr)
    {
        return false;
    }

    //============================
    // ▼ ここは一切変更していない
    //============================
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

    if (!RegisterClassEx(&wc))
    {
        return false;
    }

    m_hInst = hInst;

    RECT rc = {};
    rc.right = static_cast<LONG>(m_Width);
    rc.bottom = static_cast<LONG>(m_Height);

    auto style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    AdjustWindowRect(&rc, style, FALSE);

    m_hWnd = CreateWindowEx(
        0,
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

    ShowWindow(m_hWnd, SW_SHOWNORMAL);
    UpdateWindow(m_hWnd);
    SetFocus(m_hWnd);

    //------------------------------------------
    // ★追加：Game初期化
    //------------------------------------------
    game.Initialize();

    return true;
}

//---------------------------------------------------------------------------
// 終了処理
//---------------------------------------------------------------------------
void Application::UninitApp()
{
    //------------------------------------------
    // ★追加：Game終了
    //------------------------------------------
    game.Shutdown();

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
    long long oldTick = GetTickCount64();    //前回計測時の時間
    long long nowTick = oldTick;            //今回計測時の時間

    //----------------------------------------------------------
    // 高精度タイマー設定
    //----------------------------------------------------------
    LARGE_INTEGER liWork;
    long long frequency;

    QueryPerformanceFrequency(&liWork);
    frequency = liWork.QuadPart;

    QueryPerformanceCounter(&liWork);
    long long oldCount = liWork.QuadPart;    // Update用基準時間
    long long nowCount = oldCount;

    //----------------------------------------------------------
    // ★追加：描画制御用タイマー
    //----------------------------------------------------------
    long long renderCount = oldCount;

    //----------------------------------------------------------
    // フレーム時間（60fps）
    //----------------------------------------------------------
    const long long frameStep = frequency / 60;

    //ActiveFullScreen(true);

    //----------------------------------------------------------
    // ゲームループ
    //----------------------------------------------------------
    while (1)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if (msg.message == WM_QUIT) {
                break;
            }
        }
        else
        {
            //--------------------------------------------------
            // 現在時間取得
            //--------------------------------------------------
            QueryPerformanceCounter(&liWork);
            nowCount = liWork.QuadPart;

            //--------------------------------------------------
            // ★ Update（フレーム完全固定）
            //--------------------------------------------------
            // 変更点：
            // if → while に変更してフレーム落ち時も補正
            //--------------------------------------------------
            while (nowCount >= oldCount + frameStep)
            {
                //----------------------------------
                // Update処理
                //----------------------------------
                game.Update();

                //----------------------------------
                // FPSカウント
                //----------------------------------
                fpsCounter++;

                //----------------------------------
                // 次フレームへ進める
                //----------------------------------
                oldCount += frameStep;
            }

            //--------------------------------------------------
            // ★ Draw（最大60fps制限）
            //--------------------------------------------------
            // 追加理由：
            // 毎ループDrawするとGPU負荷が無駄に高くなるため
            //--------------------------------------------------
            if (nowCount >= renderCount + frameStep)
            {
                //----------------------------------
                // 描画処理
                //----------------------------------
                game.Draw();

                //----------------------------------
                // 描画タイミング更新
                //----------------------------------
                renderCount += frameStep;
            }

            //--------------------------------------------------
            // ★CPU負荷軽減
            //--------------------------------------------------
            // 重要：
            // これがないとCPUが100%張り付く
            //--------------------------------------------------
            Sleep(1);
        }
    }

    //シーン処理終了
    //描画処理終了
}


//---------------------------------------------------------------------------
// Destroy（変更なし）
//---------------------------------------------------------------------------
void Application::Destroy()
{
    DestroyWindow(GetWindow());
}

//---------------------------------------------------------------------------
// WndProc（完全にそのまま）
//---------------------------------------------------------------------------
LRESULT CALLBACK Application::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static bool isFullscreen = false;
    static bool isMessageBoxShowed = false;

    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_CLOSE:
    {
        int res = MessageBoxA(NULL, "終了しますか？", "確認", MB_OKCANCEL);
        if (res == IDOK) {
            DestroyWindow(hWnd);
        }
    }
    break;

    case WM_KEYDOWN:
        if (LOWORD(wParam) == VK_ESCAPE)
        {
            PostMessage(hWnd, WM_CLOSE, wParam, lParam);
        }
        else if (LOWORD(wParam) == VK_F11)
        {
            isFullscreen = !isFullscreen;
            ActiveFullScreen(isFullscreen);
        }
        break;

    case WM_ACTIVATE:
        if (wParam == WA_INACTIVE) {
            if (isFullscreen && !isMessageBoxShowed)
            {
                ShowWindow(hWnd, SW_MINIMIZE);
            }
        }
        return DefWindowProc(hWnd, uMsg, wParam, lParam);

    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            // TODO: RendererのResize対応
        }
        break;

    default:
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
        break;
    }

    return 0;
}

//---------------------------------------------------------------------------
// フルスクリーン（変更なし）
//---------------------------------------------------------------------------
void Application::ActiveFullScreen(bool _isFullScreen)
{
    if (_isFullScreen)
    {
        SetWindowLongPtr(Application::GetWindow(), GWL_STYLE, WS_POPUP | WS_MINIMIZEBOX);
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        SetWindowPos(Application::GetWindow(), HWND_TOP, 0, 0, screenWidth, screenHeight, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
    else
    {
        SetWindowLongPtr(Application::GetWindow(), GWL_STYLE, WS_OVERLAPPEDWINDOW);
        SetWindowPos(Application::GetWindow(), HWND_TOP, 100, 100, m_Width, m_Height, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
}
