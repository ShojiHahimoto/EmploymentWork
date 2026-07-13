#pragma once

#include <sstream>
#include <string>

class Debugger
{
public:
	static Debugger& GetInstance();

	void DebugLog(const std::string& message);

	// Streams all arguments into one line, then appends a newline in DebugLog(message).
	// Use: DebugLog("Name=", name, " Count=", count);
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

inline void DebugLog(const std::string& message)
{
	Debugger::GetInstance().DebugLog(message);
}

template <class... Args>
inline void DebugLog(const Args&... args)
{
	Debugger::GetInstance().DebugLog(args...);
}
