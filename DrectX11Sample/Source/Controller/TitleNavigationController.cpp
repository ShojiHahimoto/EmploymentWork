#include "Controller/TitleNavigationController.h"

#include "Input/InputSystem.h"
#include "Input/InputTypes.h"
#include "Scene/BattleScene.h"
#include "Scene/BattleSetupScene.h"
#include "Scene/CustomizeScene.h"
#include "Scene/SceneManager.h"
#include "System/Application.h"

#include <Windows.h>

#include <memory>

/// <summary>
/// タイトル画面で使う UI 入力マップを有効にする。
/// </summary>
void TitleNavigationController::Enter()
{
	Input::InputSystem::SetActionMap(Input::InputActionMapId::UI);
	customizeKeyPressedLastFrame = false;
	battleShortcutKeyPressedLastFrame = false;
}

/// <summary>
/// メニューで確定した選択と開発用ショートカットから遷移先を決める。
/// </summary>
/// <param name="menuCommand">メニュー更新側が確定した操作。</param>
void TitleNavigationController::RunSystems(TitleMenuCommand menuCommand)
{
	if (WasCustomizeTriggered() || menuCommand == TitleMenuCommand::OpenCustomize)
	{
		RequestCustomizeScene();
		return;
	}

	if (WasBattleShortcutTriggered())
	{
		RequestBattleScene();
		return;
	}

	if (menuCommand == TitleMenuCommand::StartBattleSetup)
	{
		RequestBattleSetupScene();
	}
}

/// <summary>
/// 技調整画面へ入る P キーの押下開始を検出する。
/// </summary>
/// <returns>P キーが今フレーム押された場合は true。</returns>
bool TitleNavigationController::WasCustomizeTriggered()
{
	const bool pressed = (GetAsyncKeyState('P') & 0x8000) != 0;
	const bool triggered = pressed && !customizeKeyPressedLastFrame;
	customizeKeyPressedLastFrame = pressed;
	return triggered;
}

/// <summary>
/// セットアップを飛ばす開発用 B キーの押下開始を検出する。
/// </summary>
/// <returns>B キーが今フレーム押された場合は true。</returns>
bool TitleNavigationController::WasBattleShortcutTriggered()
{
	const bool pressed = (GetAsyncKeyState('B') & 0x8000) != 0;
	const bool triggered = pressed && !battleShortcutKeyPressedLastFrame;
	battleShortcutKeyPressedLastFrame = pressed;
	return triggered;
}

/// <summary>
/// 通常のバトル開始先である BattleSetupScene への切り替えを予約する。
/// </summary>
void TitleNavigationController::RequestBattleSetupScene()
{
	SceneManager::GetInstance().RequestChangeScene(
		std::make_unique<BattleSetupScene>(
			static_cast<int>(Application::GetWidth()),
			static_cast<int>(Application::GetHeight())));
}

/// <summary>
/// タイトルから CustomizeScene への切り替えを予約する。
/// </summary>
void TitleNavigationController::RequestCustomizeScene()
{
	SceneManager::GetInstance().RequestChangeScene(
		std::make_unique<CustomizeScene>(
			static_cast<int>(Application::GetWidth()),
			static_cast<int>(Application::GetHeight())));
}

/// <summary>
/// B キー用に BattleSetupScene を経由しないバトル開始を予約する。
/// </summary>
void TitleNavigationController::RequestBattleScene()
{
	SceneManager::GetInstance().RequestChangeScene(
		std::make_unique<BattleScene>(
			static_cast<int>(Application::GetWidth()),
			static_cast<int>(Application::GetHeight())));
}
