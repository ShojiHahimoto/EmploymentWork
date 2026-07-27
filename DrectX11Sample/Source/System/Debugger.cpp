#include "System/Debugger.h"

#include <Windows.h>

#include <iostream>
#include <string>

namespace
{
	/// <summary>
	/// 指定コードページの std::string を wide 文字列へ変換する。
	/// </summary>
	/// <param name="message">変換する文字列。</param>
	/// <param name="codePage">変換元として扱う Windows コードページ。</param>
	/// <param name="flags">MultiByteToWideChar に渡す変換フラグ。</param>
	/// <returns>変換に成功した wide 文字列。失敗時は空文字列。</returns>
	std::wstring ConvertToWide(const std::string& message, UINT codePage, DWORD flags)
	{
		if (message.empty())
		{
			return L"";
		}

		const int wideLength = MultiByteToWideChar(
			codePage,
			flags,
			message.c_str(),
			static_cast<int>(message.size()),
			nullptr,
			0);
		if (wideLength <= 0)
		{
			return L"";
		}

		std::wstring wideMessage(static_cast<size_t>(wideLength), L'\0');
		MultiByteToWideChar(
			codePage,
			flags,
			message.c_str(),
			static_cast<int>(message.size()),
			wideMessage.data(),
			wideLength);
		return wideMessage;
	}

	/// <summary>
	/// DebugLog に渡された文字列を、UTF-8 優先で Windows 表示用 wide 文字列へ変換する。
	/// </summary>
	/// <param name="message">DebugLog に渡された文字列。</param>
	/// <returns>Windows API へ渡す wide 文字列。</returns>
	std::wstring ConvertLogMessageToWide(const std::string& message)
	{
		std::wstring wideMessage = ConvertToWide(message, CP_UTF8, MB_ERR_INVALID_CHARS);
		if (!wideMessage.empty() || message.empty())
		{
			return wideMessage;
		}

		// ソース文字列などが環境依存コードページで渡された場合だけ、念のため ANSI として再変換する。
		wideMessage = ConvertToWide(message, CP_ACP, 0);
		if (!wideMessage.empty())
		{
			return wideMessage;
		}

		// どちらでも変換できない場合は、ASCII 範囲だけでも読めるように最低限 widening する。
		return std::wstring(message.begin(), message.end());
	}

	/// <summary>
	/// コンソールがある場合は wide 文字列としてログを出力する。
	/// </summary>
	/// <param name="message">改行込みで出力する wide 文字列。</param>
	/// <returns>WriteConsoleW で出力できた場合は true。</returns>
	bool WriteConsoleWide(const std::wstring& message)
	{
		HANDLE outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
		if (outputHandle == nullptr || outputHandle == INVALID_HANDLE_VALUE)
		{
			return false;
		}

		DWORD consoleMode = 0;
		if (!GetConsoleMode(outputHandle, &consoleMode))
		{
			return false;
		}

		DWORD written = 0;
		return WriteConsoleW(
			outputHandle,
			message.c_str(),
			static_cast<DWORD>(message.size()),
			&written,
			nullptr) != FALSE;
	}
}

/// <summary>
/// Debugger の唯一のインスタンスを取得する。
/// </summary>
/// <returns>Debugger の参照。</returns>
Debugger& Debugger::GetInstance()
{
	static Debugger instance;
	return instance;
}

/// <summary>
/// デバッグ用メッセージをコンソールと Visual Studio の出力ウィンドウへ出力する。
/// </summary>
/// <param name="message">出力するメッセージ。</param>
void Debugger::DebugLog(const std::string& message)
{
	const std::wstring wideLine = ConvertLogMessageToWide(message) + L"\n";
	if (!WriteConsoleWide(wideLine))
	{
		std::cout << message << std::endl;
	}

	OutputDebugStringW(wideLine.c_str());
}
