#include "Scene/ResultScene.h"

#include "Input/InputSystem.h"
#include "Input/InputTypes.h"
#include "Scene/SceneManager.h"
#include "Scene/TitleScene.h"
#include "System/Application.h"
#include "System/Debugger.h"
#include "System/Renderer.h"

#include <memory>

/// <summary>
/// リザルト画面を現在の描画サイズとバトル結果で初期化する。
/// </summary>
/// <param name="initialWidth">初期ウィンドウ幅。</param>
/// <param name="initialHeight">初期ウィンドウ高さ。</param>
/// <param name="result">BattleScene で確定した勝敗結果。</param>
ResultScene::ResultScene(int initialWidth, int initialHeight, BattleResult result)
	: battleResult(result)
	, width(initialWidth)
	, height(initialHeight)
{
}

/// <summary>
/// リザルト用の入力マップへ切り替え、仮背景画像を読み込む。
/// </summary>
void ResultScene::Enter()
{
	Input::InputSystem::SetActionMap(Input::InputActionMapId::UI);

	const HRESULT hr = Renderer::LoadTextureFromFile("assets/texture/result_kari.png", &backgroundTexture);
	if (FAILED(hr))
	{
		DebugLog("[ResultScene] Background texture load failed. hr=", static_cast<long>(hr));
	}

	DebugLog("[ResultScene] Enter. BattleResult=", static_cast<int>(battleResult));
}

/// <summary>
/// リザルト用に読み込んだ描画リソースと World を破棄する。
/// </summary>
void ResultScene::Exit()
{
	Renderer::ReleaseTexture(backgroundTexture);
	world.Clear();
}

/// <summary>
/// UI Submit が押されたら TitleScene への切り替えを予約する。
/// </summary>
void ResultScene::RunSystems()
{
	if (!WasSubmitTriggered())
	{
		return;
	}

	SceneManager::GetInstance().RequestChangeScene(
		std::make_unique<TitleScene>(
			static_cast<int>(Application::GetWidth()),
			static_cast<int>(Application::GetHeight())));
}

/// <summary>
/// リザルト仮画像を画面全体へ描画する。
/// </summary>
/// <param name="renderer">描画に使用する Renderer。</param>
void ResultScene::Draw(Renderer& renderer)
{
	(void)renderer;
	Renderer::DrawFullscreenTexture(backgroundTexture, width, height);
}

/// <summary>
/// ウィンドウサイズ変更後の全画面画像描画サイズを更新する。
/// </summary>
/// <param name="newWidth">新しい幅。</param>
/// <param name="newHeight">新しい高さ。</param>
void ResultScene::OnResize(int newWidth, int newHeight)
{
	if (newWidth <= 0 || newHeight <= 0)
	{
		return;
	}

	width = newWidth;
	height = newHeight;
}

/// <summary>
/// ResultScene が保持する World を取得する。
/// </summary>
/// <returns>変更可能な World。</returns>
World& ResultScene::GetWorld()
{
	return world;
}

/// <summary>
/// ResultScene が保持する World を読み取り専用で取得する。
/// </summary>
/// <returns>読み取り専用の World。</returns>
const World& ResultScene::GetWorld() const
{
	return world;
}

/// <summary>
/// UI 操作用に、どちらかの PlayerInputState で Submit が Trigger されたか確認する。
/// </summary>
/// <returns>Submit が今フレーム押された場合は true。</returns>
bool ResultScene::WasSubmitTriggered() const
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
