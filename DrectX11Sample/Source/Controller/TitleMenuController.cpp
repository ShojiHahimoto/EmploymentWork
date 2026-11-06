#include "Controller/TitleMenuController.h"

#include "System/imgui-docking/imgui.h"

/// <summary>
/// タイトルメニューを描画し、クリックされた項目を次の更新へ渡す。
/// </summary>
/// <param name="initialWidth">初期ウィンドウ幅。</param>
/// <param name="initialHeight">初期ウィンドウ高さ。</param>
TitleMenuController::TitleMenuController(int initialWidth, int initialHeight)
	: width(initialWidth)
	, height(initialHeight)
{
}

/// <summary>
/// タイトルメニューを描画し、クリックされた項目を次の更新へ渡す。
/// </summary>
void TitleMenuController::Draw()
{
	const ImVec2 windowSize(360.0f, 190.0f);
	const ImVec2 windowPos(
		(static_cast<float>(width) - windowSize.x) * 0.5f,
		(static_cast<float>(height) - windowSize.y) * 0.5f);

	ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

	constexpr ImGuiWindowFlags windowFlags =
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings;

	if (ImGui::Begin("Title", nullptr, windowFlags))
	{
		ImGui::TextUnformatted("Build the Battle");
		ImGui::Separator();

		if (ImGui::Button("Battle Start", ImVec2(-1.0f, 42.0f)))
		{
			pendingCommand = TitleMenuCommand::StartBattleSetup;
		}

		if (ImGui::Button("Customize Scene", ImVec2(-1.0f, 42.0f)))
		{
			pendingCommand = TitleMenuCommand::OpenCustomize;
		}
	}
	ImGui::End();
}

/// <summary>
/// ウィンドウサイズ変更後にメニュー配置用サイズを更新する。
/// </summary>
/// <param name="newWidth">新しい幅。</param>
/// <param name="newHeight">新しい高さ。</param>
void TitleMenuController::OnResize(int newWidth, int newHeight)
{
	if (newWidth <= 0 || newHeight <= 0)
	{
		return;
	}

	width = newWidth;
	height = newHeight;
}

/// <summary>
/// 描画中に選択されたメニュー項目を一度だけ取得する。
/// </summary>
/// <returns>保留中のメニュー操作。</returns>
TitleMenuCommand TitleMenuController::ConsumeCommand()
{
	const TitleMenuCommand command = pendingCommand;
	pendingCommand = TitleMenuCommand::None;
	return command;
}
