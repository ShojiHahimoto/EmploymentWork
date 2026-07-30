#pragma once

#include <sstream>
#include <string>

class Debugger
{
public:
	static Debugger& GetInstance();

	void DebugLog(const std::string& message);

	/// <summary>
	/// 複数の値を 1 行の文字列に連結して DebugLog に出力する。
	/// </summary>
	/// <param name="args">std::ostringstream に流し込む任意個数の値。</param>
	template <class... Args>
	void DebugLog(const Args&... args)
	{
		std::ostringstream stream;
		(stream << ... << args);
		DebugLog(stream.str());
	}

private:
	Debugger() = default;
	Debugger(const Debugger&) = delete;
	Debugger& operator=(const Debugger&) = delete;
};

/// <summary>
/// Debugger のシングルトンへ文字列ログを出力するためのショートカット。
/// </summary>
/// <param name="message">出力するメッセージ。</param>
inline void DebugLog(const std::string& message)
{
	Debugger::GetInstance().DebugLog(message);
}

/// <summary>
/// Debugger のシングルトンへ複数値ログを出力するためのショートカット。
/// </summary>
/// <param name="args">std::ostringstream に流し込む任意個数の値。</param>
template <class... Args>
inline void DebugLog(const Args&... args)
{
	Debugger::GetInstance().DebugLog(args...);
}
