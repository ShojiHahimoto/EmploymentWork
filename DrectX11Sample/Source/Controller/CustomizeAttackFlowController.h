#pragma once

#include "Controller/CustomizeAttackEditorController.h"
#include "Controller/CustomizeMotionEditorController.h"
#include "Controller/CustomizePreviewController.h"
#include "Customize/CustomizeEditHistory.h"
#include "UI/CustomizeEditorUI.h"

#include <string>
#include <future>
#include <variant>

class LoadWorker;
class SaveWorker;

struct CustomizeAttackSlot
{
	CustomizeAttackCategory category;
	int index;
};

struct CustomizeCommonMotionSlot
{
	std::string name;
};

using CustomizeAttackTarget = std::variant<CustomizeAttackSlot, CustomizeCommonMotionSlot>;

struct PreparedAttackDraft
{
	CustomizeAttackEditorController attack;
	CustomizeMotionEditorController motion;
	std::string status;
	std::string commonMotionId;
};

class CustomizeAttackFlowController
{
  public:
	CustomizeAttackFlowController(int initialWidth, int initialHeight, CustomizeAttackTarget target);

	void Enter(LoadWorker *worker, SaveWorker *writer);
	static PreparedAttackDraft PrepareDraft(const CustomizeAttackTarget &target);
	void ApplyPreparedDraft(PreparedAttackDraft &&prepared);
	const CustomizeAttackTarget &GetTarget() const;
	void Exit();
	void Update();
	void DrawBackdrop(Renderer &renderer);
	void DrawPreview(Renderer &renderer);
	void DrawInterface(Renderer &renderer);
	void OnResize(int newWidth, int newHeight);

  private:
	CustomizeAttackTarget target;
	int width;
	int height;
	CustomizeMode mode = CustomizeMode::AttackEditor;

	CustomizeAttackEditorController attackEditor;
	CustomizeEditorUI editorUI;
	CustomizeMotionEditorController motionEditor;
	CustomizeEditHistory editHistory;
	CustomizePreviewController previewController;

	DirectX::SimpleMath::Vector2 attackMovementKeyOffset = DirectX::SimpleMath::Vector2::Zero;
	bool editingCommonMotion = false;
	std::string editingCommonMotionId;
	std::string statusMessage;
	SaveWorker *saveWorker = nullptr;
	std::future<std::string> pendingSave;

	void NavigateBack();
	void UpdateAttackEditorControls();
	CustomizeEditSnapshot CaptureEditSnapshot();
	void RestoreEditSnapshot(const CustomizeEditSnapshot &snapshot);
	void EnsureDraftAttackMotionDataId();
	void SaveDraftMotion();
	void ApplySaveResult();
	void UpdatePreviewPlayback();
	void RenderAttackPreview(Renderer &renderer, const RECT *region);
	void ClampPreviewCurrentFrame();

	int GetPreviewTotalFrames() const;
	int GetPreviewActionFrame() const;
	std::string GetEditingMotionDataId() const;
};
