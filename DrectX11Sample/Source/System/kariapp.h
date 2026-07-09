#pragma once

#include <Windows.h>
#include <cstdint>

//------------------------------------------
// Applicationクラス
//------------------------------------------
// 役割
// ・OS（Windows）とのやり取り
// ・ウィンドウ管理
// ・メインループの実行
//
// ※ゲームロジックは持たない（Gameクラスに任せる）
//------------------------------------------
class Application
{
public:
    // コンストラクタ
    // → 画面サイズを受け取る
    Application(uint32_t width, uint32_t height);

    // デストラクタ
    // → タイマー設定を元に戻す
    ~Application();

    // アプリケーション開始
    void Run();

    //------------------------------------------
    // 各情報取得（どこからでも参照できる）
    //------------------------------------------

    static uint32_t GetWidth() { return m_Width; }
    static uint32_t GetHeight() { return m_Height; }
    static HWND GetWindow() { return m_hWnd; }

private:
    //------------------------------------------
    // OS系データ
    //------------------------------------------
    static HINSTANCE m_hInst;  // アプリのインスタンス（Windowsが持つ管理情報）
    static HWND m_hWnd;        // ウィンドウハンドル（操作用ID）

    static uint32_t m_Width;
    static uint32_t m_Height;

    //------------------------------------------
    // 内部処理
    //------------------------------------------
    static bool InitApp();     // 初期化
    static void UninitApp();   // 終了処理
    static void MainLoop();    // メインループ

    //------------------------------------------
    // Windowsのメッセージ処理
    //------------------------------------------
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);

    //------------------------------------------
    // フルスクリーン切替
    //------------------------------------------
    static void ActiveFullScreen(bool isFullScreen);
};
