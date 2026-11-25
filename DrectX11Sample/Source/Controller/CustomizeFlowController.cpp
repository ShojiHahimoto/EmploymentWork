#include "Controller/CustomizeFlowController.h"

#pragma execution_character_set("utf-8")
#include "Scene/CustomizeAttackScene.h"
#include "Scene/CustomizeCharacterScene.h"
#include "Input/InputSystem.h"
#include "Input/InputTypes.h"
#include "Manager/SceneManager.h"
#include "Manager/LoadWorker.h"
#include "Core/Debugger.h"
#include <chrono>
#include "Scene/TitleScene.h"
#include "ThirdParty/imgui-docking/imgui.h"
#include "Input/UIInputSystem.h"
#include "UI/UIRenderer.h"
#include <cmath>

#include <algorithm>

namespace
{

constexpr const char *CategoryLabels[] = {"Ground", "Air", "Special"};
constexpr const char *CommonMotionLabels[] = {"Idle",		 "WalkForward", "WalkBack", "Crouch",  "Guard",
											  "CrouchGuard", "JumpStart",	"JumpLoop", "Hitstun", "AirHitstun",
											  "AirToDown",	 "Down",		"Wakeup"};

/// <summary>
/// enumのCustomizeAttackCategoryを配列アクセス用にintに変換する。
/// </summary>
/// <param name="category">変換するカテゴリ。</param>
/// <returns>Ground=0, Air=1, Special=2。</returns>
int ToCategoryIndex(CustomizeAttackCategory category)
{
	return static_cast<int>(category);
}
} // namespace

/// <summary>指定した階層とカテゴリで選択画面を準備する。</summary>
/// <param name="initialWidth">初期画面幅。</param>
/// <param name="initialHeight">初期画面高さ。</param>
/// <param name="initialMode">選択画面の開始階層。</param>
/// <param name="category">選択中の技カテゴリ。</param>
CustomizeFlowController::CustomizeFlowController(int initialWidth, int initialHeight, CustomizeMode initialMode,
	CustomizeAttackCategory category)
	: mode(initialMode), selectedCategory(category), width(initialWidth), height(initialHeight)
{
}

/// <summary>選択画面の入力とスロット一覧を準備する。</summary>
/// <param name="worker">一覧取得に使う共通ロードワーカー。</param>
void CustomizeFlowController::Enter(LoadWorker *worker)
{
	loadWorker = worker;
	Input::InputSystem::SetActionMap(Input::InputActionMapId::UI);

	if (mode == CustomizeMode::AttackSlotSelect)
	{
		categoryMenuSelection = ToCategoryIndex(selectedCategory);
		RefreshAttackSlotSummaries(selectedCategory);
	}
	else if (mode == CustomizeMode::CharacterSlotSelect)
	{
		creationMenuSelection = 1;
		RefreshCharacterSlotSummaries();
	}
}

void CustomizeFlowController::Update()
{
	ApplyPreparedSummaries();

	if (mode == CustomizeMode::MainMenu)
	{
		if (WasCancelTriggered())
		{
			pendingMenuRequest = -1;
			RequestTitleScene();

			return;
		}

		UpdateCreationMenu();

		return;
	}

	if (mode == CustomizeMode::AttackCategorySelect)
	{
		if (WasCancelTriggered())
		{
			NavigateBack();

			return;
		}

		UpdateAttackCategorySelect();

		return;
	}

	if (WasCancelTriggered())
	{
		NavigateBack();
	}
}

void CustomizeFlowController::DrawInterface(Renderer &renderer)
{
	switch (mode)
	{
	case CustomizeMode::MainMenu:
		DrawSelectionMenu(renderer, creationMenuSelection, false);
#if defined(_DEBUG)
		DrawDebugMenu();
#endif
		break;
	case CustomizeMode::AttackCategorySelect:
		DrawSelectionMenu(renderer, categoryMenuSelection, true);
		break;
	case CustomizeMode::AttackSlotSelect:
		DrawAttackSlotSelect();
		break;
	case CustomizeMode::CommonMotionSelect:
		DrawCommonMotionSelect();
		break;
	case CustomizeMode::CharacterSlotSelect:
		DrawCharacterSlotSelect();
		break;
	default:
		DrawSelectionMenu(renderer, creationMenuSelection, false);
		break;
	}
}

/// <summary>
/// ウィンドウサイズ変更後の ImGui 配置用サイズを更新する。
/// </summary>
/// <param name="newWidth">新しい幅。</param>
/// <param name="newHeight">新しい高さ。</param>
void CustomizeFlowController::OnResize(int newWidth, int newHeight)
{
	if (newWidth <= 0 || newHeight <= 0)
	{
		return;
	}

	width = newWidth;
	height = newHeight;
}

/// <summary>
/// UI Cancel が押されたか確認する。
/// </summary>
/// <returns>Cancel が今フレーム押された場合は true。</returns>
bool CustomizeFlowController::WasCancelTriggered()
{
	for (int playerIndex = 0; playerIndex < Input::MaxPlayers; ++playerIndex)
	{
		const Input::InputActionState &cancel =
			Input::InputSystem::GetActionState(playerIndex, Input::InputActionId::Cancel);
		if (cancel.trigger)
		{
			return true;
		}
	}

	return false;
}

/// <summary>
/// タイトルシーンへの切り替えを予約する。
/// </summary>
void CustomizeFlowController::RequestTitleScene()
{
	SceneManager::GetInstance().RequestChangeScene(std::make_unique<TitleScene>(width, height));
}

/// <summary>
/// 現在のカスタマイズ階層から一段戻る。
/// </summary>
void CustomizeFlowController::NavigateBack()
{
	switch (mode)
	{
	case CustomizeMode::MainMenu:
		RequestTitleScene();
		break;
	case CustomizeMode::AttackCategorySelect:
		mode = CustomizeMode::MainMenu;
		break;
	case CustomizeMode::AttackSlotSelect:
		mode = CustomizeMode::AttackCategorySelect;
		break;
	case CustomizeMode::CommonMotionSelect:
		mode = CustomizeMode::MainMenu;
		break;
	case CustomizeMode::CharacterSlotSelect:
		mode = CustomizeMode::MainMenu;
		break;
	default:
		mode = CustomizeMode::MainMenu;
		break;
	}
}

/// <summary>
/// 開発用の汎用モーション編集入口だけを描画する。ゲーム用UIとは独立させる。
/// </summary>
#if defined(_DEBUG)
void CustomizeFlowController::DrawDebugMenu()
{
	// 開発用なのでゲーム用UIとは分ける、デバッグビルドのみ有効。
	if (ImGui::Begin("Customize Debug"))
	{
		if (ImGui::Button("Common Motion Editor"))
		{
			pendingCommonMotionRequest = true;
		}
	}

	ImGui::End();
}

#endif

/// <summary>表示方式によらず、メニュー選択を次の更新へ予約する。</summary>
/// <param name="character">trueはキャラクター作成、falseは技作成。</param>
void CustomizeFlowController::RequestCreationMenu(bool character)
{
	creationMenuSelection = character ? 1 : 0;
	pendingMenuRequest = creationMenuSelection;
}

/// <summary>共通UIアクションと表示側の要求を消費する。遷移処理をImGuiから切り離す。</summary>
void CustomizeFlowController::UpdateCreationMenu()
{
	UpdateMenuPointer(creationMenuInput, GameUI::UIInputSystem::GetFrame(), creationMenuSelection, false);

	int uiRequest = -1;

	if (creationMenuInput.WasActivated("back"))
	{
		uiRequest = 2;
	}
	else
	{
		for (int index = 0; index < 2; ++index)
		{
			if (creationMenuInput.WasActivated(std::to_string(index)))
			{
				uiRequest = index;
			}
		}
	}

	if (uiRequest == 2)
	{
		RequestTitleScene();

		return;
	}

	if (uiRequest >= 0)
	{
		RequestCreationMenu(uiRequest == 1);
	}

#if defined(_DEBUG)
	if (pendingCommonMotionRequest)
	{
		pendingCommonMotionRequest = false;
		mode = CustomizeMode::CommonMotionSelect;

		return;
	}
#endif

	const auto &move = Input::InputSystem::GetActionState(0, Input::InputActionId::Move);
	const auto &submit = Input::InputSystem::GetActionState(0, Input::InputActionId::Submit);
	const int direction = move.value.axis.y > 0.5f ? -1 : move.value.axis.y < -0.5f ? 1 : 0;

	if (direction && direction != previousMenuDirection)
	{
		creationMenuSelection = direction > 0 ? 1 : 0;
	}

	previousMenuDirection = direction;

	if (!submit.press)
	{
		menuSubmitReady = true;
	}

	if (menuSubmitReady && submit.trigger)
	{
		pendingMenuRequest = creationMenuSelection;
	}

	const int request = pendingMenuRequest;
	pendingMenuRequest = -1;

	if (request < 0)
	{
		return;
	}

	menuSubmitReady = false;

	if (request == 1)
	{
		RefreshCharacterSlotSummaries();
		mode = CustomizeMode::CharacterSlotSelect;
	}
	else
	{
		categorySubmitReady = false;
		previousCategoryDirection = 0;
		categoryMenuSelection = static_cast<int>(selectedCategory);
		mode = CustomizeMode::AttackCategorySelect;
	}
}

/// <summary>
/// 専用UIと共通アクションの操作からカテゴリを確定する。描画時には遷移しない。
/// </summary>
void CustomizeFlowController::UpdateAttackCategorySelect()
{
	UpdateMenuPointer(categoryMenuInput, GameUI::UIInputSystem::GetFrame(), categoryMenuSelection, true);

	int request = -1;

	if (categoryMenuInput.WasActivated("back"))
	{
		request = 3;
	}
	else
	{
		for (int index = 0; index < 3; ++index)
		{
			if (categoryMenuInput.WasActivated(std::to_string(index)))
			{
				request = index;
			}
		}
	}

	if (request == 3)
	{
		NavigateBack();

		return;
	}

	const auto &move = Input::InputSystem::GetActionState(0, Input::InputActionId::Move);
	const auto &submit = Input::InputSystem::GetActionState(0, Input::InputActionId::Submit);
	const int direction = move.value.axis.y > 0.5f ? -1 : move.value.axis.y < -0.5f ? 1 : 0;

	if (direction && direction != previousCategoryDirection)
	{
		categoryMenuSelection = std::clamp(categoryMenuSelection + direction, 0, 2);
	}

	previousCategoryDirection = direction;

	if (!submit.press)
	{
		categorySubmitReady = true;
	}

	if (request < 0 && categorySubmitReady && submit.trigger)
	{
		request = categoryMenuSelection;
	}

	if (request < 0)
	{
		return;
	}

	selectedCategory = static_cast<CustomizeAttackCategory>(request);
	RefreshAttackSlotSummaries(selectedCategory);
	categorySubmitReady = false;
	mode = CustomizeMode::AttackSlotSelect;
}

/// <summary>
/// 選択中カテゴリ内の技スロット一覧を描画する。
/// </summary>
void CustomizeFlowController::DrawAttackSlotSelect()
{
	ImGui::SetNextWindowPos(ImVec2(40.0f, 40.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(560.0f, 420.0f), ImGuiCond_FirstUseEver);

	if (ImGui::Begin("Attack Slots"))
	{
		ImGui::Text("Category: %s", CategoryLabels[ToCategoryIndex(selectedCategory)]);

		ImGui::Separator();

		if (ImGui::Button("Refresh Slot Names", ImVec2(160.0f, 28.0f)))
		{
			RefreshAttackSlotSummaries(selectedCategory);
		}

		ImGui::Separator();

		const int slotCount = GetAttackSlotCount(selectedCategory);

		for (int slotIndex = 0; slotIndex < slotCount; ++slotIndex)
		{
			ImGui::PushID(slotIndex);

			const std::string label = BuildAttackSlotButtonLabel(selectedCategory, slotIndex);

			if (ImGui::Button(label.c_str(), ImVec2(126.0f, 52.0f)))
			{
				SelectAttackSlot(selectedCategory, slotIndex);
			}

			ImGui::PopID();

			if ((slotIndex + 1) % 4 != 0)
			{
				ImGui::SameLine();
			}
		}

		ImGui::Separator();

		if (ImGui::Button("Back", ImVec2(120.0f, 28.0f)))
		{
			NavigateBack();
		}
	}

	ImGui::End();
}

/// <summary>
/// 汎用モーションを選択し、MotionData 単体の編集画面へ入る。
/// </summary>
void CustomizeFlowController::DrawCommonMotionSelect()
{
	ImGui::SetNextWindowPos(ImVec2(40.0f, 40.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(420.0f, 460.0f), ImGuiCond_FirstUseEver);

	if (ImGui::Begin("Common Motion Select"))
	{
		for (int slotIndex = 0; slotIndex < CommonMotionSlotCount; ++slotIndex)
		{
			ImGui::PushID(slotIndex);

			if (ImGui::Button(CommonMotionLabels[slotIndex], ImVec2(260.0f, 32.0f)))
			{
				SelectCommonMotionSlot(slotIndex);
			}

			ImGui::PopID();
		}

		ImGui::Separator();

		if (ImGui::Button("Back", ImVec2(120.0f, 28.0f)))
		{
			NavigateBack();
		}
	}

	ImGui::End();
}

/// <summary>キャラクタースロット一覧を描画し、選択されたスロットの編集を予約する。</summary>
void CustomizeFlowController::DrawCharacterSlotSelect()
{
	const auto selection = characterEditor.DrawSlotSelect();

	if (selection.backRequested)
	{
		NavigateBack();

		return;
	}

	if (selection.refreshRequested)
	{
		RefreshCharacterSlotSummaries();
	}

	if (selection.slotIndex)
	{
		SceneManager::GetInstance().RequestChangeScene(
			std::make_unique<CustomizeCharacterScene>(width, height, *selection.slotIndex));
	}
}

/// <summary>技の編集シーンへの遷移を予約する。</summary>
/// <param name="category">技のカテゴリ。</param>
/// <param name="slotIndex">カテゴリ内のスロット番号。</param>
void CustomizeFlowController::SelectAttackSlot(CustomizeAttackCategory category, int slotIndex)
{
	SceneManager::GetInstance().RequestChangeScene(
		std::make_unique<CustomizeAttackScene>(width, height, CustomizeAttackSlot{category, slotIndex}));
}

/// <summary>汎用モーションの編集シーンへの遷移を予約する。</summary>
/// <param name="slotIndex">汎用モーション一覧内の番号。</param>
void CustomizeFlowController::SelectCommonMotionSlot(int slotIndex)
{
	SceneManager::GetInstance().RequestChangeScene(
		std::make_unique<CustomizeAttackScene>(width, height, CustomizeCommonMotionSlot{CommonMotionLabels[slotIndex]}));
}

/// <summary>
/// 技カテゴリごとのスロット数を取得する。
/// </summary>
/// <param name="category">確認する技カテゴリ。</param>
/// <returns>対象カテゴリのスロット数。</returns>
int CustomizeFlowController::GetAttackSlotCount(CustomizeAttackCategory category) const
{
	return attackEditor.GetSlotCount(category);
}

/// <summary>
/// 選択カテゴリのスロット JSON を確認し、一覧表示用の保存済み名を更新する。
/// </summary>
/// <param name="category">更新する技カテゴリ。</param>
void CustomizeFlowController::RefreshAttackSlotSummaries(CustomizeAttackCategory category)
{
	if (loadWorker)
	{
		pendingAttackSummaries = loadWorker->Enqueue([category]
		{
			CustomizeAttackEditorController data;
			data.RefreshSlotSummaries(category);

			return data;
		});
	}
}

/// <summary>キャラクター一覧の読み込みを共通ワーカーへ依頼する。</summary>
void CustomizeFlowController::RefreshCharacterSlotSummaries()
{
	if (loadWorker)
	{
		pendingCharacterSummaries = loadWorker->Enqueue([]
		{
			CustomizeCharacterEditorController data;
			data.RefreshSlotSummaries();

			return data;
		});
	}
}

/// <summary>一覧が表示中の場合だけ完了結果を反映する。</summary>
void CustomizeFlowController::ApplyPreparedSummaries()
{
	const auto ready = [](const auto &future)
	{
		return future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
	};

	try
	{
		if (mode != CustomizeMode::AttackSlotSelect)
		{
			pendingAttackSummaries = {};
		}
		else if (ready(pendingAttackSummaries))
		{
			attackEditor = pendingAttackSummaries.get();
		}

		if (mode != CustomizeMode::CharacterSlotSelect)
		{
			pendingCharacterSummaries = {};
		}
		else if (ready(pendingCharacterSummaries))
		{
			characterEditor = pendingCharacterSummaries.get();
		}
	}
	catch (const std::exception &error)
	{
		DebugLog("[Customize] Slot list load failed: ", error.what());
	}
}

/// <summary>
/// スロット番号と保存済み技名をまとめた、ImGui Button 用ラベルを作る。
/// </summary>
/// <param name="category">表示する技カテゴリ。</param>
/// <param name="slotIndex">カテゴリ内スロット番号。</param>
/// <returns>表示名と ImGui ID を含むボタンラベル。</returns>
std::string CustomizeFlowController::BuildAttackSlotButtonLabel(CustomizeAttackCategory category, int slotIndex) const
{
	return attackEditor.BuildSlotButtonLabel(category, slotIndex);
}

void CustomizeFlowController::UpdateMenuPointer(GameUI::Root &root, const GameUI::InputFrame &frame, int &selection,
												bool categories)
{
	using namespace GameUI;
	InputFrame pointer = frame;
	pointer.focusMove = 0;
	pointer.activatePressed = false;
	pointer.keysPressed.fill(false);

	root.BeginFrame({0, 0, width, height}, pointer);
	root.AddButton("back", MapMenuRect({18, 12, 96, 66}));

	const int count = categories ? 3 : 2;

	for (int i = 0; i < count; ++i)
	{
		root.AddButton(std::to_string(i), MapMenuRect(MenuButtonRect(i, categories)));
	}

	root.EndFrame();

	if (frame.pointerDelta.x || frame.pointerDelta.y || frame.pointerPressed)
	{
		for (int i = 0; i < count; ++i)
		{
			if (MapMenuRect(MenuButtonRect(i, categories)).Contains(frame.pointerPosition))
			{
				selection = i;
			}
		}
	}
}

void CustomizeFlowController::DrawSelectionMenu(Renderer &renderer, int selection, bool categories)
{
	using namespace GameUI;

	std::vector<DrawCommand> commands;
	DrawCommand background;
	background.bounds = {0, 0, width, height};
	background.color = DirectX::SimpleMath::Color(0.96f, 0.965f, 0.98f, 1);
	commands.push_back(background);

	const auto image = [&](const char *id, const Rect &bounds, bool slice = false)
	{
		DrawCommand c;
		c.type = slice ? DrawCommandType::NineSliceImage : DrawCommandType::Image;
		c.bounds = MapMenuRect(bounds).ToNativeRect();
		c.imageId = id;

		commands.push_back(c);
	};

	const auto text = [&](const wchar_t *value, const Rect &bounds, float size, bool selected, bool centered)
	{
		DrawCommand c;
		c.type = DrawCommandType::Text;
		c.bounds = MapMenuRect(bounds).ToNativeRect();
		c.text = value;
		c.fontSize = size;
		c.bold = true;
		c.color =
			selected ? DirectX::SimpleMath::Color(1, 1, 1, 1) : DirectX::SimpleMath::Color(0.08f, 0.09f, 0.12f, 1);
		c.horizontalAlignment = centered ? TextHorizontalAlignment::Center : TextHorizontalAlignment::Near;

		const float measured = UIRenderer::MeasureText(c.text, size, true);
		const float available = static_cast<float>(c.bounds.right - c.bounds.left);

		if (measured > available && measured > 0)
		{
			c.fontSize *= std::max(0.0f, available) / measured;
		}

		commands.push_back(c);
	};

	image("header", {0, 0, 1672, 80}, true);
	image("header_accent", {103, 0, 155, 80});
	image("button", {18, 12, 96, 66}, true);
	image("back", {37, 25, 78, 53});
	text(categories ? L"技作成" : L"カスタマイズ", {166, 2, 820, 77}, 48, false, false);

	// 上下の透明余白を含めて配置し、可視輪郭を上の固定入力矩形に合わせる。
	const wchar_t *labels[] = {L"地上技", L"空中技", L"必殺技"};

	for (int i = 0; i < (categories ? 3 : 2); ++i)
	{
		const Rect button = MenuButtonRect(i, categories);
		image(selection == i ? "customize_menu_selected" : "customize_menu_normal",
			  {button.left - 5, button.top - 54, button.right + 5, button.top + 206});
		text(categories ? labels[i]
			 : i == 0	? L"技作成"
						: L"キャラクター作成",
			 button.Inset(20, 0), 40, selection == i, true);
	}

	UIRenderer::Draw(renderer, commands);
}

GameUI::Rect CustomizeFlowController::MenuButtonRect(int index, bool categories) const
{
	const int top = (categories ? 220 : 299) + index * (categories ? 190 : 210);

	return {91, top, 617, top + 136};
}

/// <summary>画像比率を維持して基準1672x941画面を中央配置する。</summary>
/// <param name="r">基準矩形。</param>
/// <returns>画面上の矩形。</returns>
GameUI::Rect CustomizeFlowController::MapMenuRect(const GameUI::Rect &r) const
{
	const float s = std::min(width / 1672.0f, height / 941.0f);
	const float x = (width - 1672 * s) / 2;
	const float y = (height - 941 * s) / 2;

	return {static_cast<int>(std::lround(x + r.left * s)), static_cast<int>(std::lround(y + r.top * s)),
			static_cast<int>(std::lround(x + r.right * s)), static_cast<int>(std::lround(y + r.bottom * s))};
}
