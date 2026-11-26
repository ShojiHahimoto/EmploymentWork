#include "Controller/CustomizeFlowController.h"

#pragma execution_character_set("utf-8")
#include "Scene/CustomizeAttackScene.h"
#include "Scene/CustomizeCharacterScene.h"
#include "Input/InputSystem.h"
#include "Input/InputTypes.h"
#include "Manager/SceneManager.h"
#include "Manager/LoadWorker.h"
#include "Manager/SaveWorker.h"
#include "Data/CharacterDataLoader.h"
#include <filesystem>
#include "Core/Debugger.h"
#include <chrono>
#include "Scene/TitleScene.h"
#if defined(_DEBUG)
#include "ThirdParty/imgui-docking/imgui.h"
#endif
#include "Input/UIInputSystem.h"
#include "UI/UIRenderer.h"
#include <cmath>

#include <algorithm>

namespace
{

#if defined(_DEBUG)
constexpr const char *CommonMotionLabels[] = {"Idle",		 "WalkForward", "WalkBack", "Crouch",  "Guard",
											  "CrouchGuard", "JumpStart",	"JumpLoop", "Hitstun", "AirHitstun",
											  "AirToDown",	 "Down",		"Wakeup"};
#endif

/// <summary>
/// enumのCustomizeAttackCategoryを配列アクセス用にintに変換する。
/// </summary>
/// <param name="category">変換するカテゴリ。</param>
/// <returns>Ground=0, Air=1, Special=2。</returns>
int ToCategoryIndex(CustomizeAttackCategory category)
{
	return static_cast<int>(category);
}

/// <summary>片方だけ削除されることを避けるため、保存ファイルを先に退避する。</summary>
/// <param name="first">一つ目の保存ファイル。</param>
/// <param name="second">二つ目の保存ファイル。</param>
void RemoveSlotFiles(const std::filesystem::path &first, const std::filesystem::path &second)
{
	namespace fs = std::filesystem;
	auto staged = first;
	staged += ".deleting";
	const bool hasFirst = fs::exists(first);
	if (hasFirst)
	{
		fs::rename(first, staged);
	}

	try
	{
		fs::remove(second);
	}
	catch (...)
	{
		if (hasFirst)
		{
			fs::rename(staged, first);
		}

		throw;
	}

	if (hasFirst)
	{
		std::error_code error;
		fs::remove(staged, error);
		if (error)
		{
			DebugLog("[Customize] Could not remove staged slot file: ", staged.string());
		}
	}
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
/// <param name="writer">削除に使う共通保存ワーカー。</param>
void CustomizeFlowController::Enter(LoadWorker *worker, SaveWorker *writer)
{
	loadWorker = worker;
	saveWorker = writer;
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

	if (mode == CustomizeMode::AttackSlotSelect || mode == CustomizeMode::CharacterSlotSelect)
	{
		UpdateSlots();

		return;
	}

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
	case CustomizeMode::CharacterSlotSelect:
		DrawSlots(renderer);
		break;
#if defined(_DEBUG)
	case CustomizeMode::CommonMotionSelect:
		DrawCommonMotionSelect();
		break;
#endif
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
		ResetSlots();
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
	ResetSlots();
}


/// <summary>
/// 汎用モーションを選択し、MotionData 単体の編集画面へ入る。
/// </summary>
#if defined(_DEBUG)
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
#endif


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
#if defined(_DEBUG)
void CustomizeFlowController::SelectCommonMotionSlot(int slotIndex)
{
	SceneManager::GetInstance().RequestChangeScene(
		std::make_unique<CustomizeAttackScene>(width, height, CustomizeCommonMotionSlot{CommonMotionLabels[slotIndex]}));
}
#endif


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
		slotStatus = L"一覧を読み込めませんでした";
	}
}



/// <summary>キャラクターの一覧を表示中か確認する。</summary>
/// <returns>キャラクター一覧なら true。</returns>
bool CustomizeFlowController::IsCharacterList() const
{
	return mode == CustomizeMode::CharacterSlotSelect;
}

/// <summary>一覧の読み込みまたは削除が完了していないか確認する。</summary>
/// <returns>スロット操作を待機させる間は true。</returns>
bool CustomizeFlowController::SlotsLoading() const
{
	return pendingAttackSummaries.valid() || pendingCharacterSummaries.valid() || pendingDelete.valid();
}

/// <summary>指定スロットに保存データがあるか確認する。</summary>
/// <param name="slot">ページをまたいだスロット番号。</param>
/// <returns>保存済みなら true。</returns>
bool CustomizeFlowController::SlotOccupied(int slot) const
{
	return IsCharacterList() ? characterEditor.GetSlotSummary(slot).hasSavedData :
		attackEditor.GetSlotSummary(selectedCategory, slot).hasSavedData;
}

/// <summary>一覧に入る際にページと操作欄の状態を初期化する。</summary>
void CustomizeFlowController::ResetSlots()
{
	slotPage = 0;
	slotFocus = 0;
	slotAction = -1;
	slotDirection = 0;
	slotHighlighted = true;
	confirmDelete = false;
	slotStatus.clear();
	slotInput = {};
}

/// <summary>ページ内のスロットの基準座標を取得する。</summary>
/// <param name="index">ページ内の番号。</param>
/// <returns>描画と入力に共通する矩形。</returns>
GameUI::Rect CustomizeFlowController::SlotRect(int index) const
{
	const bool character = IsCharacterList();
	const int columns = character ? 2 : 5;
	const int x = 78 + (index % columns) * (character ? 770 : 307);
	const int y = (character ? 171 : 210) + (index / columns) * (character ? 314 : 145);

	return {x, y, x + (character ? 746 : 295), y + (character ? 296 : 138)};
}

/// <summary>展開中の操作ボタンの基準座標を取得する。</summary>
/// <param name="index">操作欄内の番号。</param>
/// <returns>操作ボタンの矩形。</returns>
GameUI::Rect CustomizeFlowController::SlotActionRect(int index) const
{
	const auto card = SlotRect(slotFocus);
	const int slot = slotPage * (IsCharacterList() ? 4 : 20) + slotFocus;
	const int buttonWidth = SlotOccupied(slot) ? 142 : 218;
	const int halfWidth = SlotOccupied(slot) ? buttonWidth + 3 : buttonWidth / 2;
	const int x = (card.left + card.right) / 2 - halfWidth + index * (buttonWidth + 6);
	const int top = IsCharacterList() && slotFocus >= 2 ? card.bottom - 12 : card.top - 72;

	return {x, top, x + buttonWidth, top + 64};
}

/// <summary>スロット一覧のポインターと共通アクションを処理する。</summary>
void CustomizeFlowController::UpdateSlots()
{
	if (pendingDelete.valid() && pendingDelete.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
	{
		int result = 2;
		try
		{
			result = pendingDelete.get();
		}
		catch (const std::exception &error)
		{
			DebugLog("[Customize] Slot deletion failed: ", error.what());
		}

		slotStatus = result == 0 ? L"削除しました" :
			result == 1 ? L"割り当て中の技は削除できません" : L"削除できませんでした";
		if (IsCharacterList())
		{
			RefreshCharacterSlotSummaries();
		}
		else
		{
			RefreshAttackSlotSummaries(selectedCategory);
		}
	}

	const bool character = IsCharacterList();
	const int columns = character ? 2 : 5;
	const int count = character ? 4 : 20;
	const int lastPage = character ? 2 : 1;
	const int slot = slotPage * count + slotFocus;
	const bool occupied = SlotOccupied(slot);
	const int actions = occupied ? 2 : 1;
	auto frame = GameUI::UIInputSystem::GetFrame();
	frame.focusMove = 0;
	frame.activatePressed = false;
	frame.keysPressed.fill(false);

	slotInput.BeginFrame({0, 0, width, height}, frame);
	slotInput.AddButton("back", MapMenuRect({18, 12, 96, 66}));
	if (!SlotsLoading())
	{
		for (int i = 0; i < count; ++i)
		{
			slotInput.AddButton(std::to_string(i), MapMenuRect(SlotRect(i)));
		}
	}

	if (slotAction >= 0)
	{
		for (int i = 0; i < actions; ++i)
		{
			slotInput.AddButton("action/" + std::to_string(i), MapMenuRect(SlotActionRect(i)));
		}
	}
	else
	{
		if (slotPage > 0)
		{
			slotInput.AddButton("prev", MapMenuRect({700, 833, 780, 895}));
		}

		if (slotPage < lastPage)
		{
			slotInput.AddButton("next", MapMenuRect({892, 833, 972, 895}));
		}

	}

	slotInput.EndFrame();

	if (WasCancelTriggered() || slotInput.WasActivated("back"))
	{
		if (confirmDelete)
		{
			confirmDelete = false;
		}
		else if (slotAction >= 0)
		{
			slotAction = -1;
		}
		else if (!pendingDelete.valid())
		{
			NavigateBack();
		}

		return;
	}

	if (SlotsLoading())
	{
		return;
	}

	const auto axis = Input::InputSystem::GetActionState(0, Input::InputActionId::Move).value.axis;
	const int horizontal = axis.x > .5f ? 1 : axis.x < -.5f ? -1 : 0;
	const int vertical = axis.y > .5f ? -1 : axis.y < -.5f ? 1 : 0;
	const int direction = vertical ? vertical * columns : horizontal;
	const bool moved = direction != 0 && direction != slotDirection;
	slotDirection = direction;
	const bool submit = Input::InputSystem::GetActionState(0, Input::InputActionId::Submit).trigger;
	const bool pointerMoved = frame.pointerDelta.x || frame.pointerDelta.y;

	for (int i = 0; i < count; ++i)
	{
		if (slotInput.WasActivated(std::to_string(i)))
		{
			slotFocus = i;
			slotHighlighted = true;
			slotAction = 0;
			confirmDelete = false;

			return;
		}
	}

	if (slotAction >= 0)
	{
		bool pointerOnAction = false;
		for (int i = 0; i < actions; ++i)
		{
			if (MapMenuRect(SlotActionRect(i)).Contains(frame.pointerPosition))
			{
				pointerOnAction = true;
				if (pointerMoved)
				{
					slotAction = i;
				}
			}
		}

		if (frame.pointerReleased && !pointerOnAction)
		{
			slotAction = -1;
			confirmDelete = false;
			slotHighlighted = false;

			return;
		}

		if (moved && horizontal)
		{
			slotAction = std::clamp(slotAction + horizontal, 0, actions - 1);
		}

		int action = submit ? slotAction : -1;
		for (int i = 0; i < actions; ++i)
		{
			if (slotInput.WasActivated("action/" + std::to_string(i)))
			{
				action = i;
			}
		}

		if (action < 0)
		{
			return;
		}

		if (confirmDelete)
		{
			if (action == 1)
			{
				DeleteSlot();
			}
			else
			{
				confirmDelete = false;
			}
		}
		else if (action == 1)
		{
			slotAction = 0;
			confirmDelete = true;
		}
		else if (character)
		{
			SceneManager::GetInstance().RequestChangeScene(
				std::make_unique<CustomizeCharacterScene>(width, height, slot));
		}
		else
		{
			SelectAttackSlot(selectedCategory, slot);
		}

		return;
	}

	if (slotInput.WasActivated("prev") || (moved && horizontal < 0 && slotFocus % columns == 0))
	{
		if (slotPage > 0)
		{
			--slotPage;
			slotFocus = slotFocus / columns * columns + columns - 1;
		}
	}
	else if (slotInput.WasActivated("next") ||
		(moved && horizontal > 0 && slotFocus % columns == columns - 1))
	{
		if (slotPage < lastPage)
		{
			++slotPage;
			slotFocus -= slotFocus % columns;
		}
	}
	else if (moved)
	{
		const int nextSlot = slotFocus + direction;
		if (nextSlot >= 0 && nextSlot < count)
		{
			slotFocus = nextSlot;
		}
	}

	if (pointerMoved || frame.pointerReleased)
	{
		slotHighlighted = false;
		for (int i = 0; i < count; ++i)
		{
			if (MapMenuRect(SlotRect(i)).Contains(frame.pointerPosition))
			{
				slotFocus = i;
				slotHighlighted = true;

				break;
			}
		}
	}

	if (moved || submit)
	{
		slotHighlighted = true;
	}

	if (submit)
	{
		slotAction = 0;
	}
}

/// <summary>確認済みスロットの保存ファイルを削除するよう依頼する。</summary>
void CustomizeFlowController::DeleteSlot()
{
	if (!saveWorker)
	{
		slotStatus = L"削除できませんでした";

		return;
	}

	const bool character = IsCharacterList();
	const int slot = slotPage * (character ? 4 : 20) + slotFocus;
	const auto folder = characterEditor.BuildCharacterFolderPath(slot);
	const auto id = attackEditor.BuildAttackDataId(selectedCategory, slot);
	const auto motionId = attackEditor.BuildMotionDataId(selectedCategory, slot);

	pendingDelete = saveWorker->Enqueue([character, folder, id, motionId]
	{
		namespace fs = std::filesystem;
		if (character)
		{
			const fs::path path(folder);
			RemoveSlotFiles(path / "AttackList.json", path / "Parameter.json");

			return 0;
		}

		for (const auto &entry : fs::directory_iterator("assets/CharacterData"))
		{
			if (!entry.is_directory() || !fs::exists(entry.path() / "AttackList.json"))
			{
				continue;
			}

			if (CharacterDataLoader::IsAttackReferenced(entry.path().string(), id))
			{
				return 1;
			}
		}

		RemoveSlotFiles(fs::path("assets/MotionData") / (motionId + ".json"),
			fs::path("assets/AttackData") / (id + ".json"));

		return 0;
	});
	slotAction = -1;
	confirmDelete = false;
	slotStatus.clear();
}


/// <summary>技とキャラクターで共通のスロット一覧を描画する。</summary>
/// <param name="renderer">専用UIの描画先。</param>
void CustomizeFlowController::DrawSlots(Renderer &renderer) const
{
	using namespace GameUI;
	std::vector<DrawCommand> commands;
	DrawCommand background;
	background.bounds = {0, 0, width, height};
	background.color = DirectX::SimpleMath::Color(.96f, .965f, .98f, 1);
	commands.push_back(background);

	const auto image = [&](const char *id, Rect bounds, bool slice = false, float opacity = 1)
	{
		DrawCommand command;
		command.type = slice ? DrawCommandType::NineSliceImage : DrawCommandType::Image;
		command.bounds = MapMenuRect(bounds).ToNativeRect();
		command.imageId = id;
		command.color.w = opacity;
		commands.push_back(command);
	};

	const auto fill = [&](Rect bounds, const DirectX::SimpleMath::Color &color)
	{
		DrawCommand command;
		command.bounds = MapMenuRect(bounds).ToNativeRect();
		command.color = color;
		commands.push_back(command);
	};

	const auto text = [&](std::wstring value, Rect bounds, float size, bool selected = false,
						 bool wrap = false, bool bold = true)
	{
		DrawCommand command;
		command.type = DrawCommandType::Text;
		command.bounds = MapMenuRect(bounds).ToNativeRect();
		command.clip = command.bounds;
		command.text = std::move(value);
		command.bold = bold;
		command.wrapText = wrap;
		command.fontSize = size * std::min(width / 1672.0f, height / 941.0f);
		command.horizontalAlignment = TextHorizontalAlignment::Center;
		command.color = selected ? DirectX::SimpleMath::Color(1, 1, 1, 1) :
			DirectX::SimpleMath::Color(.08f, .09f, .12f, 1);
		if (!wrap)
		{
			const float measured = UIRenderer::MeasureText(command.text, command.fontSize, bold);
			const auto available = command.bounds.right - command.bounds.left;
			if (measured > available && measured > 0)
			{
				command.fontSize *= available / measured;
			}
		}

		commands.push_back(command);
	};

	const auto wide = [](const std::string &source)
	{
		const int length = MultiByteToWideChar(CP_UTF8, 0, source.data(), static_cast<int>(source.size()), nullptr, 0);
		std::wstring result(length, L'\0');
		if (length > 0)
		{
			MultiByteToWideChar(CP_UTF8, 0, source.data(), static_cast<int>(source.size()), result.data(), length);
		}

		return result;
	};

	const bool character = IsCharacterList();
	const int count = character ? 4 : 20;
	const int pageCount = character ? 3 : 2;
	image("header", {0, 0, 1672, 80}, true);
	image("header_accent", {103, 0, 155, 80});
	image("button", {18, 12, 96, 66}, true);
	image("back", {37, 25, 78, 53});
	text(character ? L"キャラクター作成" : L"技作成", {157, 0, character ? 605 : 385, 80}, 48);

	const wchar_t *groups[] = {L"地上技", L"空中技", L"必殺技"};
	const char *badges[] = {"badge_a", "badge_b", "badge_x", "badge_y"};
	for (int i = 0; i < count; ++i)
	{
		const int slot = slotPage * count + i;
		const bool focused = i == slotFocus && slotHighlighted && !SlotsLoading();
		const auto card = SlotRect(i);
		image(focused ? "slot_focused" : "button", card, true);

		if (SlotsLoading())
		{
			continue;
		}

		if (!SlotOccupied(slot))
		{
			text(L"空", card, character ? 56.0f : 44.0f, focused);

			continue;
		}

		const int x = card.left;
		const int y = card.top;
		if (character)
		{
			const auto &summary = characterEditor.GetSlotSummary(slot);
			const Rect title = {x + 20, y + 16, card.right - 20, y + 97};
			image(focused ? "slot_focused" : "button", title, true);
			text(wide(summary.characterName), title.Inset(12, 0), 40, focused, true);
			for (int row = 0; row < 3; ++row)
			{
				const int top = y + 102 + row * 54;
				const Rect category = {x + 25, top, x + 115, top + 49};
				fill(category, {.78f, .81f, .85f, .96f});
				text(groups[row], category, 21);
				for (int button = 0; button < 4; ++button)
				{
					const int left = x + 121 + button * 151;
					const auto &name = summary.attackNames[row][button];
					fill({left, top, left + 145, top + 49}, {.91f, .92f, .95f, .96f});
					image(badges[button], {left + 5, top + 6, left + 42, top + 43});
					text(name.empty() ? L"空" : wide(name), {left + 45, top, left + 142, top + 49},
						18, false, true);
				}
			}
		}
		else
		{
			const auto &summary = attackEditor.GetSlotSummary(selectedCategory, slot);
			text(wide(summary.displayName), {x + 12, y + 9, card.right - 12, y + 90}, 34, focused, true);
			const auto &frame = summary.frame;
			text(L"発生 " + std::to_wstring(frame.startup) + L"F / 持続 " + std::to_wstring(frame.active) +
				L"F / 硬直 " + std::to_wstring(frame.recovery) + L"F",
				{x + 24, y + 89, card.right - 24, card.bottom - 21}, 19, focused, false, false);
		}
	}

	image("button", {700, 833, 780, 895}, true, slotPage == 0 ? .35f : 1);
	image("back", {718, 848, 762, 880}, false, slotPage == 0 ? .35f : 1);
	image("button", {892, 833, 972, 895}, true, slotPage + 1 == pageCount ? .35f : 1);
	image("slot_forward", {910, 848, 954, 880}, false, slotPage + 1 == pageCount ? .35f : 1);
	text(std::to_wstring(slotPage + 1) + L" / " + std::to_wstring(pageCount), {782, 833, 890, 895}, 27);
	text(SlotsLoading() ? L"処理中…" : slotStatus, {1000, 833, 1650, 895}, 22);

	if (slotAction >= 0)
	{
		const int slot = slotPage * count + slotFocus;
		const bool occupied = SlotOccupied(slot);
		for (int i = 0; i < (occupied ? 2 : 1); ++i)
		{
			const auto bounds = SlotActionRect(i);
			image(i == slotAction ? "slot_focused" : "button", bounds, true);
			text(confirmDelete ? (i == 0 ? L"キャンセル" : L"削除") :
				(occupied ? (i == 0 ? L"編集" : L"削除") : L"新規作成"),
				bounds.Inset(8, 0), occupied ? 27.0f : 32.0f, i == slotAction);
		}

		if (confirmDelete)
		{
			const auto name = character ? characterEditor.GetSlotSummary(slot).characterName :
				attackEditor.GetSlotSummary(selectedCategory, slot).displayName;
			image("panel", {510, 81, 1162, 106}, true);
			text(L"「" + wide(name) + L"」を削除しますか？", {520, 81, 1152, 106}, 19);
		}
	}

	UIRenderer::Draw(renderer, commands);
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
