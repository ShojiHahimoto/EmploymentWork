#include "System/Debugger.h"

#include <Windows.h>

#include <iostream>

Debugger& Debugger::GetInstance()
{
	static Debugger instance;
	return instance;
}

void Debugger::DebugLog(const std::string& message)
{
	std::cout << message << std::endl;
	OutputDebugStringA((message + "\n").c_str());
}
