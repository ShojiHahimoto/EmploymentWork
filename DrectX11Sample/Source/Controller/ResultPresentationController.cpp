#include "Controller/ResultPresentationController.h"

#include "System/imgui-docking/imgui.h"

namespace
{
/// <summary>
/// BattleResult を画面表示用の文字列へ変換する。
/// </summary>
/// <param name="result">表示する勝敗結果。</param>
/// <returns>結果を表す短い文字列。</returns>
const char* GetResultText(BattleResult result)
{
	switch (result)
	{
	case BattleResult::Player1Win: return "Player 1 Win";
	case BattleResult::Player2Win: return "Player 2 Win";
	case BattleResult::Draw: return "Draw";
	case BattleResult::None:
	default: return "Result";
	}
}
}

/// <summary>
/// 表示対象となるバトル結果を保持する。
/// </summary>
/// <param name="battleResult">BattleScene が確定した勝敗結果。</param>
ResultPresentationController::ResultPresentationController(BattleResult battleResult, int initialWidth, int initialHeight)
	: result(battleResult)
	, width(initialWidth)
	, height(initialHeight)
{
}

/// <summary>
/// 結果表示を描画し、タイトルへ戻るボタンの操作を記録する。
/// </summary>
void ResultPresentationController::Draw()
{
	const ImVec2 windowSize(360.0f, 170.0f);
	const ImVec2 windowPos(
		(static_cast<float>(width) - windowSize.x) * 0.5f,
		(static_cast<float>(height) - windowSize.y) * 0.5f);

	ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

	constexpr ImGuiWindowFlags windowFlags =
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings;

	if (ImGui::Begin("Result", nullptr, windowFlags))
	{
		ImGui::TextUnformatted("Battle Result");
		ImGui::Separator();
		ImGui::TextUnformatted(GetResultText(result));
		ImGui::Spacing();

		if (ImGui::Button("Back To Title", ImVec2(-1.0f, 42.0f)))
		{
			returnRequested = true;
		}
	}
	ImGui::End();
}

/// <summary>
/// ウィンドウサイズ変更後に結果表示の配置用サイズを更新する。
/// </summary>
/// <param name="newWidth">新しい幅。</param>
/// <param name="newHeight">新しい高さ。</param>
void ResultPresentationController::OnResize(int newWidth, int newHeight)
{
	if (newWidth <= 0 || newHeight <= 0)
	{
		return;
	}

	width = newWidth;
	height = newHeight;
}

/// <summary>
/// 描画時に記録したタイトル復帰操作を一度だけ取得する。
/// </summary>
/// <returns>タイトル復帰が要求されていた場合は true。</returns>
bool ResultPresentationController::ConsumeReturnRequest()
{
	const bool requested = returnRequested;
	returnRequested = false;
	return requested;
}
