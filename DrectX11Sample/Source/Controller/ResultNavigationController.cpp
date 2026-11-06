#include "Controller/ResultNavigationController.h"

#include "Input/InputSystem.h"
#include "Input/InputTypes.h"
#include "Scene/SceneManager.h"
#include "Scene/TitleScene.h"
#include "System/Application.h"
#include "System/Debugger.h"

#include <memory>

/// <summary>
/// リザルト画面の遷移判断に使う勝敗結果を保持する。
/// </summary>
/// <param name="battleResult">BattleScene が確定した勝敗結果。</param>
ResultNavigationController::ResultNavigationController(BattleResult battleResult)
	: result(battleResult)
{
}

/// <summary>
/// リザルト画面用の UI 入力マップを有効にする。
/// </summary>
void ResultNavigationController::Enter()
{
	Input::InputSystem::SetActionMap(Input::InputActionMapId::UI);
	DebugLog("[ResultScene] Enter. BattleResult=", static_cast<int>(result));
}

/// <summary>
/// Submit または画面ボタンの要求を受けてタイトルへの遷移を予約する。
/// </summary>
/// <param name="returnRequestedByView">表示側でボタンが押されたか。</param>
void ResultNavigationController::RunSystems(bool returnRequestedByView)
{
	if (returnRequestedByView || WasSubmitTriggered())
	{
		RequestTitleScene();
	}
}

/// <summary>
/// いずれかのプレイヤーに割り当てられた Submit 入力を検出する。
/// </summary>
/// <returns>Submit が今フレーム押された場合は true。</returns>
bool ResultNavigationController::WasSubmitTriggered() const
{
	for (int playerIndex = 0; playerIndex < Input::MaxPlayers; ++playerIndex)
	{
		if (Input::InputSystem::GetActionState(playerIndex, Input::InputActionId::Submit).trigger)
		{
			return true;
		}
	}

	return false;
}

/// <summary>
/// リザルト画面を閉じて TitleScene へ戻る遷移を予約する。
/// </summary>
void ResultNavigationController::RequestTitleScene()
{
	SceneManager::GetInstance().RequestChangeScene(
		std::make_unique<TitleScene>(
			static_cast<int>(Application::GetWidth()),
			static_cast<int>(Application::GetHeight())));
}
