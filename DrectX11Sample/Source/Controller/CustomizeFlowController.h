#pragma once

#include "Data/AttackData.h"
#include "Controller/CustomizeAttackEditorController.h"
#include "Controller/CustomizeCharacterEditorController.h"
#include "Controller/CustomizeMotionEditorController.h"
#include "Controller/CustomizePreviewController.h"
#include "Controller/CustomizeEditHistory.h"
#include "Controller/CustomizeTypes.h"
#include "System/Renderer.h"
#include "UI/CustomizeAttackEditorView.h"
#include "UI/CustomizeMenuView.h"
#include "World/World.h"

#include <string>

class CustomizeFlowController
{
public:
	CustomizeFlowController(World& world, int initialWidth, int initialHeight);

	void Enter();
	void Exit();
	void RunSystems();
	void Draw(Renderer& renderer);
	void OnResize(int newWidth, int newHeight);
	/// <summary>表示方式によらず、メニュー選択を次の更新へ予約する。</summary>
	/// <param name="character">trueはキャラクター作成、falseは技作成。</param>
	void RequestCreationMenu(bool character);

	World& GetWorld();
	const World& GetWorld() const;

private:
	static constexpr int CommonMotionSlotCount = 13;

	World& world;
	CustomizeMode mode = CustomizeMode::MainMenu;
	int creationMenuSelection = 0; // 0:技作成、1:キャラクター作成。今後の専用UIも共有する。
	GameUI::CustomizeMenuView creationMenuView;
	int previousMenuDirection = 0;
	int pendingMenuRequest = -1; // 描画側では遷移せず更新で一度だけ消費する。
	bool menuSubmitReady = false; // 入場時の決定押しっぱなしを防ぐ。
#if defined(_DEBUG)
	bool pendingCommonMotionRequest = false;
#endif
	void UpdateCreationMenu();
	CustomizeAttackCategory selectedCategory = CustomizeAttackCategory::Ground;
	int width = 0;
	int height = 0;

	CustomizeAttackEditorController attackEditor;
	CustomizeAttackEditorView attackEditorView;
	CustomizeMotionEditorController motionEditor;
	CustomizeEditHistory editHistory; // 攻撃/モーション画面で共有し、スロットを抜けたら破棄する。
	CustomizeCharacterEditorController characterEditor;
	DirectX::SimpleMath::Vector2 attackMovementKeyOffset = DirectX::SimpleMath::Vector2::Zero;
	bool editingCommonMotion = false;
	int selectedCommonMotionIndex = 0;
	std::string editingCommonMotionId;
	std::string statusMessage;
	CustomizePreviewController previewController;

	bool WasCancelTriggered();
	void RequestTitleScene();
	void NavigateBack();

#if defined(_DEBUG)
	void DrawDebugMenu();
#endif
	void UpdateAttackCategorySelect();
	GameUI::CustomizeMenuView categoryMenuView;
	int categoryMenuSelection = 0;
	int previousCategoryDirection = 0;
	bool categorySubmitReady = false;
	void DrawAttackSlotSelect();
	void DrawCommonMotionSelect();
	void DrawAttackEditor(Renderer& renderer);
	void DrawMotionEditorScreen(Renderer& renderer);
	void UpdateAttackEditorControls();
	/// <summary>保存対象だけを複製し、現在フレームを復元用に付ける。</summary>
	/// <returns>カメラやコピー用姿勢を含まない編集状態。</returns>
	CustomizeEditSnapshot CaptureEditSnapshot();
	/// <summary>履歴の下書きを復元し、古い入力・Gizmoの捕捉を終了する。</summary>
	/// <param name="snapshot">復元する状態。</param>
	void RestoreEditSnapshot(const CustomizeEditSnapshot& snapshot);
	void SelectAttackSlot(CustomizeAttackCategory category, int slotIndex);
	void SelectCommonMotionSlot(int slotIndex);
	void EnsureDraftAttackMotionDataId();
	void LoadDraftMotionFromEditorId();
	void SaveDraftMotion();
	void InitializePreview();
	void ReleasePreview();
	void UpdatePreviewPlayback();
	void RenderAttackPreview(Renderer& renderer, const RECT* region = nullptr);
	void ClampPreviewCurrentFrame();

	int GetPreviewTotalFrames() const;
	int GetPreviewActionFrame() const;
	const char* GetPreviewPhaseText() const;
	std::string GetEditingMotionDataId() const;

	int GetAttackSlotCount(CustomizeAttackCategory category) const;
	void RefreshAttackSlotSummaries(CustomizeAttackCategory category);
	std::string BuildAttackSlotButtonLabel(CustomizeAttackCategory category, int slotIndex) const;
	std::string BuildAttackDataId(CustomizeAttackCategory category, int slotIndex) const;
	std::string BuildMotionDataId(CustomizeAttackCategory category, int slotIndex) const;
	std::string BuildCommonMotionDataId(int slotIndex) const;
	void CopyMotionEditorBuffers();
};


