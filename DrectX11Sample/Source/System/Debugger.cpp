#include "System/Debugger.h"

#include <Windows.h>

#include <iostream>

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
	std::cout << message << std::endl;
	OutputDebugStringA((message + "\n").c_str());
}
