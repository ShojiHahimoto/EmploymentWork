#include "Scene/TitleScene.h"

#include "Input/InputSystem.h"
#include "Input/InputTypes.h"
#include "Scene/BattleScene.h"
#include "Scene/BattleSetupScene.h"
#include "Scene/CustomizeScene.h"
#include "Scene/SceneManager.h"
#include "System/Application.h"
#include "System/Debugger.h"
#include "System/Renderer.h"

#include <Windows.h>

#include <memory>

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
/// タイトル用の入力マップへ切り替え、仮背景画像を読み込む。
/// </summary>
void TitleScene::Enter()
{
	Input::InputSystem::SetActionMap(Input::InputActionMapId::UI);

	const HRESULT hr = Renderer::LoadTextureFromFile("assets/texture/title_kari.png", &backgroundTexture);
	if (FAILED(hr))
	{
		DebugLog("[TitleScene] Background texture load failed. hr=", static_cast<long>(hr));
	}
}

/// <summary>
/// タイトル用に読み込んだ描画リソースと World を破棄する。
/// </summary>
void TitleScene::Exit()
{
	Renderer::ReleaseTexture(backgroundTexture);
	world.Clear();
}

/// <summary>
/// UI Submit が押されたら BattleSetupScene、B が押されたら開発用に BattleScene へ直行する。
/// </summary>
void TitleScene::RunSystems()
{
	if (WasCustomizeTriggered())
	{
		SceneManager::GetInstance().RequestChangeScene(
			std::make_unique<CustomizeScene>(
				static_cast<int>(Application::GetWidth()),
				static_cast<int>(Application::GetHeight())));
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
		SceneManager::GetInstance().RequestChangeScene(
			std::make_unique<BattleSetupScene>(
				static_cast<int>(Application::GetWidth()),
				static_cast<int>(Application::GetHeight())));
	}
}

/// <summary>
/// タイトル仮画像を画面全体へ描画する。
/// </summary>
/// <param name="renderer">描画に使用する Renderer。</param>
void TitleScene::Draw(Renderer& renderer)
{
	(void)renderer;
	Renderer::DrawFullscreenTexture(backgroundTexture, width, height);
}

/// <summary>
/// ウィンドウサイズ変更後の全画面画像描画サイズを更新する。
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
