#pragma once

#include "Controller/CustomizeAttackEditorController.h"
#include "Data/MotionSkeletonDefinition.h"
#include "UI/UIRoot.h"

class CustomizePreviewController;
class CustomizeMotionEditorController;
class Renderer;

/// <summary>技・モーション編集の共通専用UI。共通レイアウトと再生・Timelineを一度だけ実装する。</summary>
class CustomizeEditorUI
{
  public:
	CustomizeAttackEditorAction GetRequestedAction() const;
	void Reset();
	void Update(CustomizeAttackEditorController &controller, CustomizePreviewController &preview, int width, int height,
				std::string &status, CustomizeMotionEditorController &motion, bool motionScreen = false,
				bool commonMotion = false, bool canUndo = false, bool canRedo = false);

	GameUI::Root::EditInteraction GetEditInteraction() const;
	void ResetAfterHistory();

	bool IsMovementSelected() const;

	bool AreMarkersVisible() const;
	RECT PreviewToolbar(int width, int height) const;

	const std::vector<RECT> &PreviewToolbarExclusions() const;
	void Draw(Renderer &renderer) const;
	void DrawBackdrop(Renderer &renderer, int width, int height) const;
	static RECT PreviewRegion(int width, int height);

  private:
	CustomizeAttackEditorAction requestedAction = CustomizeAttackEditorAction::None;
	static std::string UserNotification(const std::string &status);
	std::vector<RECT> previewToolbarRects;
	bool suppressInputOnce = false; // 履歴復元直後の再構築では、同じ押下を二度処理しない。
	void BuildMotionControls(CustomizeMotionEditorController &motion, CustomizePreviewController &preview, bool common,
							 int top, int bottom, int split, int width);
	void ApplyMotionControls(CustomizeMotionEditorController &motion, CustomizePreviewController &preview, bool common,
							 std::string &status, const GameUI::Rect &previewRegion);
	void BuildInlinePosePreset(CustomizeMotionEditorController &motion, bool keyed, int left, int right, int &y);
	bool movementSelected = false;
	bool multipleSelectionEnabled = false;
	bool showMarkers = true; // 画面を行き来しても表示設定を維持する。
	bool motionScreenActive = false;
	bool pastePanelOpen = false;
	bool mirrorPanelOpen = false;
	bool mirrorRightToLeft = true;
	std::array<bool, 6> mirrorParts{};
	std::array<bool, 2> mirrorGroups{};
	GameUI::Rect mirrorPanelBounds{};
	GameUI::Rect pastePanelBounds{};
	int presetPanelMode = 0; // 0:閉、1:保存、2:読込。将来のモーションプリセットも同じ展開規則。
	GameUI::Rect presetPanelBounds{}; // スクロール補正後の表示範囲。フォーカス移動の判定に使う。
	int presetIndex = 0;
	int motionPanelScroll = 0;
	std::string motionName, presetName;
	std::vector<std::string> presetIds, presetNames;
	void Section(const char *text);
	GameUI::Rect Row(const std::string &id, const char *label);
	void Integer(const char *id, const char *label, int &value, int minimum, int maximum);
	void BuildParameters(CustomizeAttackCategory category);
	void BuildBehavior(CustomizeAttackCategory category);
	void BuildHitboxes();
	void BuildCancel();

	GameUI::Root root;					 // 入力捕捉と描画部品。Drawまで参照先を維持する。
	bool timelineDragging = false;
	int draggedKeyFrame = -1;
	int draggedKeyLane = 0;
	long keyPressX = 0;
	bool keyDragStarted = false;
	bool timelineKeyMoved = false;
	AttackEditState edit; // UI部品が参照する当フレームの編集スナップショット。
	int heightIndex = 0, usableIndex = 0, reactionIndex = 0, commandIndex = 0;
	int cancelStart = 1, cancelEnd = 1;
	bool normalCancel = false, specialCancel = false, jumpCancel = false;
	int scroll = 0, contentHeight = 1400, rowY = 0, panelWidth = 0;
	std::string message; // 専用画面に表示する日本語の保存結果。
};
