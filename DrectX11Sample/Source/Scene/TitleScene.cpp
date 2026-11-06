#include "Scene/TitleScene.h"

#include "Input/InputSystem.h"
#include "Input/InputTypes.h"
#include "Scene/BattleScene.h"
#include "Scene/BattleSetupScene.h"
#include "Scene/CustomizeScene.h"
#include "Scene/SceneManager.h"
#include "System/Application.h"
#include "System/Renderer.h"
#include "System/imgui-docking/imgui.h"

#include <Windows.h>

#include <memory>

namespace
{
/// <summary>
/// タイトルの通常ルートとして BattleSetupScene への切り替えを予約する。
/// </summary>
void RequestBattleSetupScene()
{
	SceneManager::GetInstance().RequestChangeScene(
		std::make_unique<BattleSetupScene>(
			static_cast<int>(Application::GetWidth()),
			static_cast<int>(Application::GetHeight())));
}

/// <summary>
/// カスタマイズ画面への切り替えを予約する。
/// </summary>
void RequestCustomizeScene()
{
	SceneManager::GetInstance().RequestChangeScene(
		std::make_unique<CustomizeScene>(
			static_cast<int>(Application::GetWidth()),
			static_cast<int>(Application::GetHeight())));
}
}

/// <summary>
/// タイトル画面を現在の描画サイズで初期化する。
/// </summary>
/// <param name="initialWidth">初期ウィンドウ幅。</param>
/// <param name="initialHeight">初期ウィンドウ高さ。</param>
TitleScene::TitleScene(int initialWidth, int initialHeight)
	: width(initialWidth)
	, height(initialHeight)
{
}

/// <summary>
/// タイトル用の入力マップへ切り替える。
/// </summary>
void TitleScene::Enter()
{
	Input::InputSystem::SetActionMap(Input::InputActionMapId::UI);
}

/// <summary>
/// タイトル用の World を破棄する。
/// </summary>
void TitleScene::Exit()
{
	world.Clear();
}

/// <summary>
/// UI Submit が押されたら BattleSetupScene、B が押されたら開発用に BattleScene へ直行する。
/// </summary>
void TitleScene::RunSystems()
{
	if (WasCustomizeTriggered())
	{
		RequestCustomizeScene();
		return;
	}

	if (WasBattleShortcutTriggered())
	{
		SceneManager::GetInstance().RequestChangeScene(
			std::make_unique<BattleScene>(
				static_cast<int>(Application::GetWidth()),
				static_cast<int>(Application::GetHeight())));
		return;
	}

	if (WasSubmitTriggered())
	{
		RequestBattleSetupScene();
	}
}

/// <summary>
/// タイトルの仮 UI を ImGui で表示する。
/// </summary>
/// <param name="renderer">描画に使用する Renderer。</param>
void TitleScene::Draw(Renderer& renderer)
{
	(void)renderer;

	const ImVec2 windowSize(360.0f, 190.0f);
	const ImVec2 windowPos(
		(static_cast<float>(width) - windowSize.x) * 0.5f,
		(static_cast<float>(height) - windowSize.y) * 0.5f);

	ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

	constexpr ImGuiWindowFlags WindowFlags =
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings;

	if (ImGui::Begin("Title", nullptr, WindowFlags))
	{
		ImGui::TextUnformatted("Build the Battle");
		ImGui::Separator();

		if (ImGui::Button("Battle Start", ImVec2(-1.0f, 42.0f)))
		{
			RequestBattleSetupScene();
		}

		if (ImGui::Button("Customize Scene", ImVec2(-1.0f, 42.0f)))
		{
			RequestCustomizeScene();
		}
	}
	ImGui::End();
}

/// <summary>
/// ウィンドウサイズ変更後の UI 配置用サイズを更新する。
/// </summary>
/// <param name="newWidth">新しい幅。</param>
/// <param name="newHeight">新しい高さ。</param>
void TitleScene::OnResize(int newWidth, int newHeight)
{
	if (newWidth <= 0 || newHeight <= 0)
	{
		return;
	}

	width = newWidth;
	height = newHeight;
}

/// <summary>
/// TitleScene が保持する World を取得する。
/// </summary>
/// <returns>変更可能な World。</returns>
World& TitleScene::GetWorld()
{
	return world;
}

/// <summary>
/// TitleScene が保持する World を読み取り専用で取得する。
/// </summary>
/// <returns>読み取り専用の World。</returns>
const World& TitleScene::GetWorld() const
{
	return world;
}

/// <summary>
/// UI 操作用に、どちらかの PlayerInputState で Submit が Trigger されたか確認する。
/// </summary>
/// <returns>Submit が今フレーム押された場合は true。</returns>
bool TitleScene::WasSubmitTriggered() const
{
	for (int playerIndex = 0; playerIndex < Input::MaxPlayers; ++playerIndex)
	{
		const Input::InputActionState& submit =
			Input::InputSystem::GetActionState(playerIndex, Input::InputActionId::Submit);
		if (submit.trigger)
		{
			return true;
		}
	}

	return false;
}

/// <summary>
/// 技調整シーンへ入る仮導線として、P キーが Trigger されたか確認する。
/// </summary>
/// <returns>P キーが今フレーム押された場合は true。</returns>
bool TitleScene::WasCustomizeTriggered()
{
	const bool pressed = (GetAsyncKeyState('P') & 0x8000) != 0;
	const bool triggered = pressed && !customizeKeyPressedLastFrame;
	customizeKeyPressedLastFrame = pressed;

	return triggered;
}

/// <summary>
/// 開発用に BattleSetupScene を経由せず BattleScene へ入る B キーが Trigger されたか確認する。
/// </summary>
/// <returns>B キーが今フレーム押された場合は true。</returns>
bool TitleScene::WasBattleShortcutTriggered()
{
	const bool pressed = (GetAsyncKeyState('B') & 0x8000) != 0;
	const bool triggered = pressed && !battleShortcutKeyPressedLastFrame;
	battleShortcutKeyPressedLastFrame = pressed;

	return triggered;
}
