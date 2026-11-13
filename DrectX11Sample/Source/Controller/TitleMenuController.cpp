#include "Controller/TitleMenuController.h"
#include "Input/InputSystem.h"
#include "UI/UIInputSystem.h"
#include "System/Renderer.h"
#include <algorithm>
#include <cmath>

/// <summary>タイトル用の画面サイズを保持する。</summary>
/// <param name="initialWidth">画面幅。</param>
/// <param name="initialHeight">画面高さ。</param>
TitleMenuController::TitleMenuController(int initialWidth, int initialHeight)
	: width(initialWidth), height(initialHeight) {}

/// <summary>入場時の初期選択と入力捕捉を初期化する。</summary>
void TitleMenuController::Enter()
{
	inputRoot = {};
	selectedIndex = previousDirection = 0;
	submitArmed = previousSubmitDown = pointerArmed = false;
	pendingCommand = TitleMenuCommand::None;
	BuildDrawCommands();
}

void TitleMenuController::Update()
{
	// UIマップは全パッドとキーボードをPlayer0へ集約済み。対戦割り当ては変えない。
	const auto& move = Input::InputSystem::GetActionState(0, Input::InputActionId::Move);
	const auto& submit = Input::InputSystem::GetActionState(0, Input::InputActionId::Submit);
	UpdateInput(GameUI::UIInputSystem::GetFrame(), move.value.axis.x, submit.press);
}

void TitleMenuController::UpdateInput(const GameUI::InputFrame& input, float horizontal, bool submitDown)
{
	const int previousSelection = selectedIndex;
	GameUI::InputFrame pointerInput = input;
	// 決定と左右選択をRootと二重実行しない。Rootにはマウス捕捉だけを委託する。
	pointerInput.focusMove = 0;
	pointerInput.activatePressed = false;
	pointerInput.keysPressed.fill(false);
	if (!input.pointerDown) pointerArmed = true;
	if (!pointerArmed) pointerInput.pointerPressed = pointerInput.pointerReleased = false;
	inputRoot.BeginFrame({0, 0, width, height}, pointerInput);
	inputRoot.AddButton("battle", GetButtonBounds(0));
	inputRoot.AddButton("customize", GetButtonBounds(1));
	inputRoot.EndFrame();
	if (input.focusLost)
	{
		submitArmed = pointerArmed = false;
		previousDirection = 0;
		previousSubmitDown = submitDown;
		return;
	}
	// 静止カーソルはパッドの選択を奪わない。
	if (input.pointerDelta.x || input.pointerDelta.y || pointerInput.pointerPressed)
		for (int i = 0; i < 2; ++i)
			if (GetButtonBounds(i).Contains(input.pointerPosition)) selectedIndex = i;
	const int direction = horizontal > 0.5f ? 1 : horizontal < -0.5f ? -1 : 0;
	if (direction && direction != previousDirection) selectedIndex = direction > 0 ? 1 : 0;
	previousDirection = direction;
	if (input.focusMove) selectedIndex = 1 - selectedIndex;
	if (!submitDown) submitArmed = true;
	const bool submit = (submitArmed && submitDown && !previousSubmitDown)
		|| input.keysPressed[static_cast<size_t>(GameUI::Key::Enter)];
	previousSubmitDown = submitDown;
	// 同フレームにクリックと決定があっても遷移要求は一つだけ。
	if (inputRoot.WasActivated("battle")) selectedIndex = 0;
	else if (inputRoot.WasActivated("customize")) selectedIndex = 1;
	else if (!submit)
	{
		if (previousSelection != selectedIndex) BuildDrawCommands();
		return;
	}
	pendingCommand = selectedIndex == 0 ? TitleMenuCommand::StartBattleSetup : TitleMenuCommand::OpenCustomize;
	if (previousSelection != selectedIndex) BuildDrawCommands();
}

/// <summary>基準画面を縦横比固定でクライアント領域に収める。</summary>
/// <param name="reference">1672x941基準の矩形。</param>
/// <returns>画面上の矩形。</returns>
GameUI::Rect TitleMenuController::MapRect(const GameUI::Rect& reference) const
{
	const float scale = std::min(width / 1672.0f, height / 941.0f);
	const float x = (width - 1672 * scale) * 0.5f, y = (height - 941 * scale) * 0.5f;
	return {static_cast<int>(std::lround(x + reference.left * scale)), static_cast<int>(std::lround(y + reference.top * scale)),
		static_cast<int>(std::lround(x + reference.right * scale)), static_cast<int>(std::lround(y + reference.bottom * scale))};
}

/// <summary>透明余白を除いた、選択状態によらず固定の操作矩形を返す。</summary>
/// <param name="index">左0、右1。</param>
/// <returns>入力範囲。</returns>
GameUI::Rect TitleMenuController::GetButtonBounds(int index) const
{
	const int x = index == 0 ? 180 : 850;
	return MapRect({x + 30, 778, x + 620, 884});
}

/// <summary>タイトル画像と文字を共通レンダラーの描画要求へ変換する。</summary>
void TitleMenuController::BuildDrawCommands()
{
	drawCommands.clear();
	// 第3段階で3D背景へ置き換える仮背景。毎フレームの再ラスタライズは不要。
	GameUI::DrawCommand logo;
	logo.type = GameUI::DrawCommandType::Image;
	logo.bounds = MapRect({190, -70, 1480, 360}).ToNativeRect();
	logo.imageId = "title_v2_logo";
	drawCommands.push_back(logo);
	for (int i = 0; i < 2; ++i)
	{
		const int x = i == 0 ? 180 : 850;
		GameUI::DrawCommand background;
		background.type = GameUI::DrawCommandType::Image;
		background.bounds = MapRect({x, 725, x + 651, 942}).ToNativeRect();
		background.imageId = selectedIndex == i ? "title_v2_selected" : "title_v2_normal";
		drawCommands.push_back(background);
		GameUI::DrawCommand label;
		label.type = GameUI::DrawCommandType::Text;
		label.bounds = GetButtonBounds(i).Inset(20, 8).ToNativeRect();
		label.text = i == 0 ? L"バトルスタート" : L"カスタマイズ";
		label.bold = true;
		label.fontSize = 36.0f;
		// 文字が収まらない画面のみ実測幅に合わせる。
		const float measured = GameUI::UIRenderer::MeasureText(label.text, label.fontSize, true);
		const float available = static_cast<float>(label.bounds.right - label.bounds.left);
		if (measured > available && measured > 0) label.fontSize *= std::max(0.0f, available) / measured;
		label.horizontalAlignment = GameUI::TextHorizontalAlignment::Center;
		label.color = selectedIndex == i ? DirectX::SimpleMath::Color(1, 1, 1, 1) : DirectX::SimpleMath::Color(0.04f, 0.04f, 0.04f, 1);
		drawCommands.push_back(label);
	}
}

/// <summary>確定済みUIを描画する。仮背景は第3段階の3D背景へ置換予定。</summary>
/// <param name="renderer">共通レンダラー。</param>
void TitleMenuController::Draw(Renderer& renderer)
{
	GameUI::UIRenderer::Draw(renderer, drawCommands);
}

/// <summary>画面サイズ変更を配置に反映する。</summary>
/// <param name="newWidth">画面幅。</param>
/// <param name="newHeight">画面高さ。</param>
void TitleMenuController::OnResize(int newWidth, int newHeight)
{
	if (newWidth <= 0 || newHeight <= 0) return;
	width = newWidth;
	height = newHeight;
	BuildDrawCommands();
}

/// <summary>遷移要求を一度だけ取り出す。</summary>
/// <returns>保留している要求。</returns>
TitleMenuCommand TitleMenuController::ConsumeCommand()
{
	const auto command = pendingCommand;
	pendingCommand = TitleMenuCommand::None;
	return command;
}
