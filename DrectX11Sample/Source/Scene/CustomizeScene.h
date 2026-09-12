#pragma once

#include "Data/AttackData.h"
#include "Controller/CustomizeAttackEditorController.h"
#include "Controller/CustomizeCharacterEditorController.h"
#include "Controller/CustomizeMotionEditorController.h"
#include "Controller/CustomizePreviewController.h"
#include "Controller/CustomizeTypes.h"
#include "Scene/Scene.h"
#include "System/Renderer.h"
#include "World/World.h"

#include <string>

class CustomizeScene : public Scene
{
public:
	CustomizeScene(int initialWidth, int initialHeight);
	~CustomizeScene() override = default;

	void Enter() override;
	void Exit() override;
	void RunSystems() override;
	void Draw(Renderer& renderer) override;
	void OnResize(int newWidth, int newHeight) override;

	World& GetWorld() override;
	const World& GetWorld() const override;

private:
	static constexpr int CommonMotionSlotCount = 13;

	World world;
	CustomizeMode mode = CustomizeMode::MainMenu;
	CustomizeAttackCategory selectedCategory = CustomizeAttackCategory::Ground;
	int width = 0;
	int height = 0;

	CustomizeAttackEditorController attackEditor;
	CustomizeMotionEditorController motionEditor;
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
	void DrawPreviewPlaybackControls();
	void DrawAttackEditorWindow();
	void DrawAttackEditorControls();
	void SelectAttackSlot(CustomizeAttackCategory category, int slotIndex);
	void SelectCommonMotionSlot(int slotIndex);
	void SaveDraftAttack();
	void SyncDraftFromEditor();
	void EnsureDraftAttackMotionDataId();
	void LoadDraftMotionFromEditorId();
	void SaveDraftMotion();
	void InitializePreview();
	void ReleasePreview();
	void UpdatePreviewPlayback();
	void RenderAttackPreview(Renderer& renderer, const RECT* region = nullptr);
	void ClampPreviewCurrentFrame();
	void StepPreviewFrame(int frameDelta);

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
