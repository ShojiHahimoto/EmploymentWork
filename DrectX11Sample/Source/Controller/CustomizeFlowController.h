#pragma once

#include "Controller/CustomizeAttackEditorController.h"
#include "Controller/CustomizeCharacterEditorController.h"
#include "Customize/CustomizeTypes.h"
#include "Core/Renderer.h"
#include "UI/UIRoot.h"
#include <future>

#include <string>

class LoadWorker;
class SaveWorker;

class CustomizeFlowController
{
  public:
	CustomizeFlowController(int initialWidth, int initialHeight, CustomizeMode initialMode,
		CustomizeAttackCategory category);

	void Enter(LoadWorker *worker, SaveWorker *writer);
	void Update();
	void DrawInterface(Renderer &renderer);
	void OnResize(int newWidth, int newHeight);
	void RequestCreationMenu(bool character);

  private:
	static constexpr int CommonMotionSlotCount = 13;

	CustomizeMode mode = CustomizeMode::MainMenu;
	int creationMenuSelection = 0; // 0:技作成、1:キャラクター作成。今後の専用UIも共有する。
	GameUI::Root creationMenuInput;
	int previousMenuDirection = 0;
	int pendingMenuRequest = -1; // imguiが残っている際、Draw内の処理で繊維しないための保持用の変数
	bool menuSubmitReady = false; // 決定入力の連続入力を防ぐ。
#if defined(_DEBUG)
	bool pendingCommonMotionRequest = false;
#endif
	void UpdateCreationMenu();
	void UpdateMenuPointer(GameUI::Root &root, const GameUI::InputFrame &frame, int &selection, bool categories);
	void DrawSelectionMenu(Renderer &renderer, int selection, bool categories);
	GameUI::Rect MenuButtonRect(int index, bool categories) const;
	GameUI::Rect MapMenuRect(const GameUI::Rect &rectangle) const;
	CustomizeAttackCategory selectedCategory = CustomizeAttackCategory::Ground;
	int width = 0;
	int height = 0;

	CustomizeAttackEditorController attackEditor;
	CustomizeCharacterEditorController characterEditor;
	LoadWorker *loadWorker = nullptr;
	std::future<CustomizeAttackEditorController> pendingAttackSummaries;
	std::future<CustomizeCharacterEditorController> pendingCharacterSummaries;

	bool WasCancelTriggered();
	void RequestTitleScene();
	void NavigateBack();

#if defined(_DEBUG)
	void DrawDebugMenu();
#endif
	void UpdateAttackCategorySelect();
	GameUI::Root categoryMenuInput;
	int categoryMenuSelection = 0;
	int previousCategoryDirection = 0;
	bool categorySubmitReady = false;
	void UpdateSlots();
	void DrawSlots(Renderer &renderer) const;
	void ResetSlots();
	void DeleteSlot();
	bool IsCharacterList() const;
	bool SlotOccupied(int slot) const;
	bool SlotsLoading() const;
	GameUI::Rect SlotRect(int index) const;
	GameUI::Rect SlotActionRect(int index) const;
	GameUI::Root slotInput;
	int slotPage = 0;
	int slotFocus = 0;
	int slotAction = -1;
	int slotDirection = 0;
	bool slotHighlighted = true;
	bool confirmDelete = false;
	std::wstring slotStatus;
	SaveWorker *saveWorker = nullptr;
	std::future<int> pendingDelete;
#if defined(_DEBUG)
	void DrawCommonMotionSelect();
	void SelectCommonMotionSlot(int slotIndex);
#endif
	void SelectAttackSlot(CustomizeAttackCategory category, int slotIndex);

	void RefreshAttackSlotSummaries(CustomizeAttackCategory category);
	void RefreshCharacterSlotSummaries();
	void ApplyPreparedSummaries();
};
