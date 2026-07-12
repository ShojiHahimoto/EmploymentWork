#pragma once

#include "Component/TransformComponent.h"
#include "World/World.h"
#include <Windows.h>
#include <d3d11.h>

class DebugImGuiSystem
{
public:
	static bool Init(HWND windowHandle, ID3D11Device* device, ID3D11DeviceContext* deviceContext);
	static void Shutdown();

	static bool IsInitialized();
	static bool HandleWndProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);

	static void BeginFrame();
	static void Render();

	static void DrawTransformEditor(const char* windowName, TransformComponent& transform);
	static void DrawWorldInspector(World& world);
	static void DrawSpawnWindow(World& world);

private:
	static bool initialized;
};
