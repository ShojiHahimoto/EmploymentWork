#pragma once

#include "UI/UIRoot.h"

#include <Windows.h>

namespace GameUI
{
	/// <summary>Win32イベントとゲーム共通UIアクションを、UI部品向けの1フレーム入力へまとめる。</summary>
	class UIInputSystem
	{
	public:
		/// <summary>WndProcから呼び、マウス・IME・文字入力イベントを次のフレーム用に蓄積する。</summary>
		/// <param name="windowHandle">メッセージを受け取ったウィンドウ。</param>
		/// <param name="message">Win32メッセージ番号。</param>
		/// <param name="wParam">メッセージのWPARAM。</param>
		/// <param name="lParam">メッセージのLPARAM。</param>
		static void HandleWindowMessage(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);
		/// <summary>蓄積イベントとUI ActionMapを統合し、現在フレーム入力を確定する。</summary>
		/// <param name="windowHandle">クライアント座標を取得するウィンドウ。</param>
		static void Update(HWND windowHandle);
		/// <summary>現在のフレームに確定したUI入力を返す。</summary>
		/// <returns>参照先は次のUpdateまで有効な入力状態。</returns>
		static const InputFrame& GetFrame();
		/// <summary>入力イベントと押下状態を初期化する。</summary>
		static void Reset();
	};
}
