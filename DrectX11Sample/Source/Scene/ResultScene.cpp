#include "Scene/ResultScene.h"

#include "Input/InputSystem.h"
#include "Input/InputTypes.h"
#include "Scene/SceneManager.h"
#include "Scene/TitleScene.h"
#include "System/Application.h"
#include "System/Debugger.h"
#include "System/Renderer.h"
#include "System/imgui-docking/imgui.h"

#include <memory>

namespace
{
/// <summary>
/// リザルト画面からタイトル画面への切り替えを予約する。
/// </summary>
void RequestTitleScene()
{
	SceneManager::GetInstance().RequestChangeScene(
		std::make_unique<TitleScene>(
			static_cast<int>(Application::GetWidth()),
			static_cast<int>(Application::GetHeight())));
}

/// <summary>
/// BattleResult をリザルト表示用の短い文字列へ変換する。
/// </summary>
/// <param name="result">BattleScene から渡された勝敗結果。</param>
/// <returns>リザルト画面に表示する文字列。</returns>
const char* GetResultText(BattleResult result)
{
	switch (result)
	{
	case BattleResult::Player1Win:
		return "Player 1 Win";
	case BattleResult::Player2Win:
		return "Player 2 Win";
	case BattleResult::Draw:
		return "Draw";
	case BattleResult::None:
	default:
		return "Result";
	}
}
}

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
/// リザルト用の入力マップへ切り替える。
/// </summary>
void ResultScene::Enter()
{
	Input::InputSystem::SetActionMap(Input::InputActionMapId::UI);

	DebugLog("[ResultScene] Enter. BattleResult=", static_cast<int>(battleResult));
}

/// <summary>
/// リザルト用の World を破棄する。
/// </summary>
void ResultScene::Exit()
{
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

	RequestTitleScene();
}

/// <summary>
/// リザルトの仮 UI を ImGui で表示する。
/// </summary>
/// <param name="renderer">描画に使用する Renderer。</param>
void ResultScene::Draw(Renderer& renderer)
{
	(void)renderer;

	const ImVec2 windowSize(360.0f, 170.0f);
	const ImVec2 windowPos(
		(static_cast<float>(width) - windowSize.x) * 0.5f,
		(static_cast<float>(height) - windowSize.y) * 0.5f);

	ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

	constexpr ImGuiWindowFlags WindowFlags =
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings;

	if (ImGui::Begin("Result", nullptr, WindowFlags))
	{
		ImGui::TextUnformatted("Battle Result");
		ImGui::Separator();
		ImGui::TextUnformatted(GetResultText(battleResult));
		ImGui::Spacing();

		if (ImGui::Button("Back To Title", ImVec2(-1.0f, 42.0f)))
		{
			RequestTitleScene();
		}
	}
	ImGui::End();
}

/// <summary>
/// ウィンドウサイズ変更後の UI 配置用サイズを更新する。
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
