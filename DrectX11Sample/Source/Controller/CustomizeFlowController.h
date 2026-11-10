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

	World& GetWorld();
	const World& GetWorld() const;

private:
	static constexpr int CommonMotionSlotCount = 13;

	World& world;
	CustomizeMode mode = CustomizeMode::MainMenu;
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

	void DrawMainMenu();
	void DrawAttackCategorySelect();
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


