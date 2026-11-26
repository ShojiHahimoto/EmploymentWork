#pragma once

#include "UI/UIRoot.h"

#include <Windows.h>

namespace GameUI
{
	/// <summary>Win32イベントとゲーム共通UIアクションを、UI部品向けの1フレーム入力へまとめる。</summary>
	class UIInputSystem
	{
	public:
		static void HandleWindowMessage(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);
		static void Update(HWND windowHandle);
		static const InputFrame& GetFrame();
		static void AddKeyboardSubmit(InputFrame& frame, bool editingText);
		static void Reset();
	};
}
