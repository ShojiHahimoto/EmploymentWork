#include "System/DebugImGuiSystem.h"

#include "Component/CameraComponent.h"
#include "Component/StateComponent.h"
#include "Component/VelocityComponent.h"
#include "System/TransformSystem.h"

#include <cstdint>

#if defined(_DEBUG)
#include "System/imgui-docking/imgui.h"
#include "System/imgui-docking/backends/imgui_impl_dx11.h"
#include "System/imgui-docking/backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND windowHandle,
	UINT message,
	WPARAM wParam,
	LPARAM lParam);
#endif

using namespace DirectX::SimpleMath;

bool DebugImGuiSystem::initialized = false;

bool DebugImGuiSystem::Init(HWND windowHandle, ID3D11Device* device, ID3D11DeviceContext* deviceContext)
{
#if defined(_DEBUG)
	if (initialized)
	{
		return true;
	}

	if (!windowHandle || !device || !deviceContext)
	{
		return false;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	ImGui::StyleColorsDark();

	// Viewport を有効にすると ImGui ウィンドウをアプリ外へ分離できる。
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	if (!ImGui_ImplWin32_Init(windowHandle))
	{
		ImGui::DestroyContext();
		return false;
	}

	if (!ImGui_ImplDX11_Init(device, deviceContext))
	{
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		return false;
	}

	initialized = true;
	return true;
#else
	(void)windowHandle;
	(void)device;
	(void)deviceContext;
	return false;
#endif
}

void DebugImGuiSystem::Shutdown()
{
#if defined(_DEBUG)
	if (!initialized)
	{
		return;
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	initialized = false;
#endif
}

bool DebugImGuiSystem::IsInitialized()
{
	return initialized;
}

bool DebugImGuiSystem::HandleWndProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
{
#if defined(_DEBUG)
	if (!initialized)
	{
		return false;
	}

	const bool handledByImGui = ImGui_ImplWin32_WndProcHandler(windowHandle, message, wParam, lParam) != 0;
	if (!handledByImGui)
	{
		return false;
	}

	// 終了、リサイズ、アクティブ状態の変化はアプリ本体にも必ず通す。
	// ImGui が入力状態を更新しても、ここで飲み込むと Game 側の管理が止まる。
	switch (message)
	{
	case WM_CLOSE:
	case WM_DESTROY:
	case WM_SIZE:
	case WM_ACTIVATE:
	case WM_ENTERSIZEMOVE:
	case WM_EXITSIZEMOVE:
		return false;
	default:
		break;
	}

	const ImGuiIO& io = ImGui::GetIO();
	switch (message)
	{
	case WM_MOUSEMOVE:
	case WM_NCMOUSEMOVE:
	case WM_MOUSELEAVE:
	case WM_NCMOUSELEAVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
	case WM_MOUSEWHEEL:
	case WM_MOUSEHWHEEL:
		return io.WantCaptureMouse;

	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_CHAR:
		return io.WantCaptureKeyboard;

	default:
		return false;
	}
#else
	(void)windowHandle;
	(void)message;
	(void)wParam;
	(void)lParam;
	return false;
#endif
}

void DebugImGuiSystem::BeginFrame()
{
#if defined(_DEBUG)
	if (!initialized)
	{
		return;
	}

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// 中央領域を透過して、ゲーム画面を ImGui の背景で覆わないようにする。
	// デバッグウィンドウだけをドッキング・分離できる状態に保つ。
	ImGui::DockSpaceOverViewport(
		0,
		nullptr,
		ImGuiDockNodeFlags_PassthruCentralNode);
#endif
}

void DebugImGuiSystem::Render()
{
#if defined(_DEBUG)
	if (!initialized)
	{
		return;
	}

	ImGuiIO& io = ImGui::GetIO();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
#endif
}

void DebugImGuiSystem::DrawTransformEditor(const char* windowName, TransformComponent& transform)
{
#if defined(_DEBUG)
	if (!initialized)
	{
		return;
	}

	ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(320.0f, 150.0f), ImGuiCond_FirstUseEver);

	if (ImGui::Begin(windowName))
	{
		Vector3 position = TransformSystem::GetLocalPosition(transform);
		Vector3 rotation = TransformSystem::GetLocalEulerRotationDegrees(transform);
		Vector3 scale = TransformSystem::GetLocalScale(transform);

		if (ImGui::DragFloat3("Position", &position.x, 0.05f))
		{
			TransformSystem::SetLocalPosition(transform, position);
		}

		if (ImGui::DragFloat3("Rotation", &rotation.x, 0.5f))
		{
			TransformSystem::SetLocalEulerRotationDegrees(transform, rotation);
		}

		if (ImGui::DragFloat3("Scale", &scale.x, 0.01f, 0.001f, 100.0f))
		{
			TransformSystem::SetLocalScale(transform, scale);
		}

		if (ImGui::Button("Reset"))
		{
			TransformSystem::SetLocalPosition(transform, Vector3::Zero);
			TransformSystem::SetLocalEulerRotationDegrees(transform, Vector3::Zero);
			TransformSystem::SetLocalScale(transform, Vector3::One);
		}
	}

	ImGui::End();
#else
	(void)windowName;
	(void)transform;
#endif
}

void DebugImGuiSystem::DrawWorldInspector(World& world)
{
#if defined(_DEBUG)
	if (!initialized)
	{
		return;
	}

	ImGui::SetNextWindowPos(ImVec2(20.0f, 190.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(420.0f, 420.0f), ImGuiCond_FirstUseEver);

	if (ImGui::Begin("World Objects"))
	{
		for (GameObject& object : world.GetGameObjects())
		{
			ImGui::PushID(static_cast<int>(object.id));

			const bool isActiveCamera = object.id == world.GetActiveCameraId();
			const char* objectName = object.name.empty() ? "GameObject" : object.name.c_str();
			char label[128] = {};
			sprintf_s(label, "%s (ID: %u)%s", objectName, object.id, isActiveCamera ? " [Active Camera]" : "");

			if (ImGui::TreeNode(label))
			{
				ImGui::Text("Components:");
				if (world.HasComponent<TransformComponent>(object.id)) ImGui::BulletText("Transform");
				if (world.HasComponent<CameraComponent>(object.id)) ImGui::BulletText("Camera");
				if (world.HasComponent<VelocityComponent>(object.id)) ImGui::BulletText("Velocity");
				if (world.HasComponent<StateComponent>(object.id)) ImGui::BulletText("State");

				TransformComponent* transform = world.GetTransform(object.id);
				if (transform)
				{
					Vector3 position = TransformSystem::GetLocalPosition(*transform);
					Vector3 rotation = TransformSystem::GetLocalEulerRotationDegrees(*transform);
					Vector3 scale = TransformSystem::GetLocalScale(*transform);

					if (ImGui::DragFloat3("Position", &position.x, 0.05f))
					{
						TransformSystem::SetLocalPosition(*transform, position);
					}

					if (ImGui::DragFloat3("Rotation", &rotation.x, 0.5f))
					{
						TransformSystem::SetLocalEulerRotationDegrees(*transform, rotation);
					}

					if (ImGui::DragFloat3("Scale", &scale.x, 0.01f, 0.001f, 100.0f))
					{
						TransformSystem::SetLocalScale(*transform, scale);
					}
				}

				if (isActiveCamera)
				{
					ImGui::BeginDisabled();
					ImGui::Button("Delete");
					ImGui::EndDisabled();
				}
				else if (ImGui::Button("Delete"))
				{
					world.RequestDestroy(object.id);
				}

				ImGui::TreePop();
			}

			ImGui::PopID();
		}
	}

	ImGui::End();
#else
	(void)world;
#endif
}

void DebugImGuiSystem::DrawSpawnWindow(World& world)
{
#if defined(_DEBUG)
	if (!initialized)
	{
		return;
	}

	static int selectedType = 0;
	static char objectName[64] = "DebugCube";
	static Vector3 position = Vector3(0.0f, 0.0f, 6.0f);
	static Vector3 rotation = Vector3::Zero;

	ImGui::SetNextWindowPos(ImVec2(460.0f, 20.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(320.0f, 160.0f), ImGuiCond_FirstUseEver);

	if (ImGui::Begin("Debug Spawn"))
	{
		struct SpawnTypeOption
		{
			const char* label;
			SpawnType type;
		};

		static constexpr SpawnTypeOption spawnTypeOptions[] =
		{
			{ "DebugCube", SpawnType::DebugCube },
			{ "Debugman", SpawnType::Debugman },
		};

		const char* spawnTypeLabels[IM_ARRAYSIZE(spawnTypeOptions)] = {};
		for (int i = 0; i < IM_ARRAYSIZE(spawnTypeOptions); ++i)
		{
			spawnTypeLabels[i] = spawnTypeOptions[i].label;
		}

		if (selectedType < 0 || selectedType >= IM_ARRAYSIZE(spawnTypeOptions))
		{
			selectedType = 0;
		}

		ImGui::Combo("Type", &selectedType, spawnTypeLabels, IM_ARRAYSIZE(spawnTypeLabels));
		ImGui::InputText("Name", objectName, IM_ARRAYSIZE(objectName));
		ImGui::DragFloat3("Position", &position.x, 0.05f);
		ImGui::DragFloat3("Rotation", &rotation.x, 0.5f);

		if (ImGui::Button("Spawn"))
		{
			world.RequestSpawn(spawnTypeOptions[selectedType].type, objectName, position, rotation);
		}
	}

	ImGui::End();
#else
	(void)world;
#endif
}

bool DebugImGuiSystem::DrawSceneView(ID3D11ShaderResourceView* sceneTextureView, int textureWidth, int textureHeight)
{
#if defined(_DEBUG)
	if (!initialized || !sceneTextureView || textureWidth <= 0 || textureHeight <= 0)
	{
		return false;
	}

	bool viewHovered = false;

	ImGui::SetNextWindowPos(ImVec2(800.0f, 20.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(480.0f, 320.0f), ImGuiCond_FirstUseEver);

	if (ImGui::Begin("Scene View"))
	{
		const ImVec2 availableSize = ImGui::GetContentRegionAvail();
		const float textureAspect = static_cast<float>(textureWidth) / static_cast<float>(textureHeight);

		ImVec2 imageSize = availableSize;
		if (imageSize.x / imageSize.y > textureAspect)
		{
			imageSize.x = imageSize.y * textureAspect;
		}
		else
		{
			imageSize.y = imageSize.x / textureAspect;
		}

		if (imageSize.x > 1.0f && imageSize.y > 1.0f)
		{
			ImGui::Image(
				static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(sceneTextureView)),
				imageSize,
				ImVec2(0.0f, 0.0f),
				ImVec2(1.0f, 1.0f));

			viewHovered = ImGui::IsItemHovered();
		}
	}

	ImGui::End();
	return viewHovered;
#else
	(void)sceneTextureView;
	(void)textureWidth;
	(void)textureHeight;
	return false;
#endif
}
