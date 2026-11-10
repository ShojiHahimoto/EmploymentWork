#include "UI/CustomizeAttackEditorView.h"
#pragma execution_character_set("utf-8")
#include "Controller/CustomizeMotionEditorController.h"
#include "Data/PosePreset.h"
#include "UI/UITheme.h"
#include "UI/UIInputSystem.h"
#include <algorithm>
#include <cstdio>

RECT CustomizeAttackEditorView::PreviewToolbar(int width, int height) const
{
    if (!motionScreenActive) return {};
    const RECT region = PreviewRegion(width, height);
    return {region.left + 4, region.top + 4, std::min(region.right, region.left + 392), region.top + 38};
}

void CustomizeAttackEditorView::BuildMotionControls(CustomizeMotionEditorController& motion,
    CustomizePreviewController& preview, bool common, int top, int bottom, int split, int width)
{
    const int inlineHeight = presetPanelMode == 0 ? 0 : 116;
    root.BeginScrollArea("motion/controls", {split + 8, top + 8, width - 8, bottom - 96}, (common ? 590 : 550) + inlineHeight, motionPanelScroll);
    const int left = 16, right = width - split - 32, gap = 16;
    const int half = (left + right) / 2;
    const int frame = preview.GetActionFrame();
    const bool keyed = motion.HasMotionKeyframe(frame);
    const auto heading = [&](const char* id, const char* label, int y)
    {
        root.AddImage(std::string(id) + "/accent", {left, y + 4, left + 7, y + 26}, GameUI::Theme::SectionAccentImage);
        root.AddLabel(id, {left + 18, y, right, y + 28}, label, true, 20, false, true);
    };
    // 既存の汎用モーション総尺・ループ設定も専用UIから編集可能に残す。
    int y = 8;
    heading("pose/heading", "ポーズキーフレーム", y); y += 42;
    root.AddIconButton("pose/add", {left, y, half - gap / 2, y + 44}, "ポーズキーを追加", "add", frame >= 0);
    root.AddIconButton("pose/delete", {half + gap / 2, y, right, y + 44}, "ポーズキーを削除", "delete", keyed);
    y += 64;
    heading("part/heading", "選択部位", y); y += 42;
    const int third = (right - left - gap * 2) / 3;
    root.AddIconButton("pose/copy", {left, y, left + third, y + 42}, "コピー", "copy", keyed);
    root.AddIconButton("pose/paste", {left + third + gap, y, left + third * 2 + gap, y + 42}, "ペースト", "paste", keyed && motion.HasCopiedPose());
    root.AddIconButton("pose/mirror", {left + (third + gap) * 2, y, right, y + 42}, "ミラーリング", "mirror");
    y += 62;
    const int presetTop = y;
    heading("preset/heading", "ポーズプリセット", y); y += 42;
    root.AddIconButton("preset/save", {left, y, half - gap / 2, y + 42}, "保存", "preset_save", keyed);
    root.AddIconButton("preset/load", {half + gap / 2, y, right, y + 42}, "読み込み", "preset_load", keyed);
    y += 52;
    BuildInlinePosePreset(motion, keyed, left, right, y);
    y += 10;
    presetPanelBounds = {split + 8 + left, top + 8 + presetTop - motionPanelScroll,
        split + 8 + right, top + 8 + y - motionPanelScroll};
    heading("motionPreset/heading", "モーションプリセット", y); y += 42;
    root.AddIconButton("motionPreset/save", {left, y, half - gap / 2, y + 42}, "保存", "preset_save");
    root.AddIconButton("motionPreset/load", {half + gap / 2, y, right, y + 42}, "読み込み", "preset_load");
    y += 62;
    heading("movement/heading", "移動キーフレーム", y); y += 42;
    root.AddIconButton("movement/add", {left, y, half - gap / 2, y + 44}, "移動キーを追加", "add", frame >= 0);
    root.AddIconButton("movement/delete", {half + gap / 2, y, right, y + 44}, "移動キーを削除", "delete", frame >= 0);
    y += 64;
    if (common)
    {
        root.AddLabel("common/total/label", {left, y, left + 90, y + 30}, "総フレーム", true, 16);
        root.AddIntegerInput("common/total", {left + 96, y, half, y + 30}, motion.GetDraft().totalFrames, 1, 100000);
        root.AddCheckbox("common/loop", {half + gap, y, right, y + 30}, "ループ", motion.GetDraft().looping);
    }
    root.EndScrollArea();
    root.AddLabel("motion/status", {split + 24, bottom - 90, width - 24, bottom - 62}, message, true, 13);
    root.AddButton("motion", {split + 48, bottom - 58, width - 48, bottom - 18}, common ? "モーション選択へ戻る" : "パラメータ編集");
}

void CustomizeAttackEditorView::ApplyMotionControls(CustomizeMotionEditorController& motion,
    CustomizePreviewController& preview, bool common, std::string& status, const GameUI::Rect& previewRegion)
{
    message = UserNotification(status);
    const int frame = preview.GetActionFrame();
    const int total = common ? motion.GetDraft().totalFrames : GetAttackTotalFrames(edit.attackData.frame);
    if (motion.IsMultipleSelectionEnabled() != multipleSelectionEnabled)
    {
        preview.CancelGizmoEditing();
        motion.SetMultipleSelectionEnabled(multipleSelectionEnabled);
    }
    if (root.WasActivated("select/all")) { preview.CancelGizmoEditing(); motion.SelectAllBodyParts(); motion.RefreshFrameEditValues(frame); }
    if (root.WasActivated("select/none")) { preview.CancelGizmoEditing(); motion.SelectBodyPart(preview, -1, frame); }
    std::snprintf(motion.GetDisplayNameBuffer().data(), motion.GetDisplayNameBuffer().size(), "%s", motionName.c_str());
    if (root.WasActivated("pose/add")) { status = motion.AddWholeBodyKeyframe(frame, total); movementSelected = false; }
    if (root.WasActivated("pose/delete")) status = motion.DeleteWholeBodyKeyframe(frame);
    if (root.WasActivated("pose/copy")) status = motion.CopyWholeBodyPose(frame);
    if (root.WasActivated("pose/paste")) status = motion.PasteWholeBodyPose(frame);
    if (root.WasActivated("gizmo/rotate")) { preview.translationGizmoEnabled = false; movementSelected = false; }
    if (root.WasActivated("gizmo/move")) { preview.translationGizmoEnabled = true; movementSelected = false; }
    if (root.WasActivated("movement/add"))
    {
        if (common) status = motion.AddRootOffsetKeyframe(frame);
        else { DirectX::SimpleMath::Vector2 value; motion.AddAttackMovementKeyframe(edit.attackData, frame, value); }
        movementSelected = frame >= 0;
    }
    if (root.WasActivated("movement/delete"))
    {
        if (common) status = motion.DeleteRootOffsetKeyframe(frame);
        else { DirectX::SimpleMath::Vector2 value; motion.DeleteAttackMovementKeyframe(edit.attackData, frame, value); }
    }
    if (root.WasActivated("preset/save")) { preview.Stop(); presetPanelMode = presetPanelMode == 1 ? 0 : 1; }
    if (root.WasActivated("preset/load"))
    {
        preview.Stop();
        presetPanelMode = presetPanelMode == 2 ? 0 : 2; presetIndex = 0;
        presetIds = {"__IdlePose__"}; presetNames = {"アイドル姿勢"};
        for (const auto& id : PosePresetStore::ListPresetIds())
        {
            PosePresetData data;
            if (PosePresetStore::LoadPreset(id, data)) { presetIds.push_back(id); presetNames.push_back(data.displayName); }
        }
    }
    // 仮ボタンは通常の濃さで描き、未実装の機能を実行したようには見せない。
    for (const char* id : {"pose/mirror", "motionPreset/save", "motionPreset/load"})
        if (root.WasActivated(id)) status = "この機能は今後実装します。";
    if (root.WasActivated("preset/confirmSave")) status = motion.SavePosePreset(frame, presetName);
    if ((root.WasActivated("preset/applyAll") || root.WasActivated("preset/applySelected"))
        && presetIndex >= 0 && presetIndex < static_cast<int>(presetIds.size()))
    {
        auto mask = motion.GetSelectedBodyParts();
        if (root.WasActivated("preset/applyAll")) mask.fill(true);
        status = motion.ApplyPosePreset(presetIds[presetIndex], frame, mask);
    }
    if (root.WasActivated("preset/cancel")) presetPanelMode = 0;
    if (presetPanelMode != 0)
    {
        // プレビューの部位選択は適用先の指定なので継続する。他項目へのフォーカスで閉じる。
        if (root.ShouldCollapseInlineSection("preset/", presetPanelBounds, previewRegion))
            presetPanelMode = 0;
    }
}

void CustomizeAttackEditorView::BuildInlinePosePreset(CustomizeMotionEditorController& motion,
    bool keyed, int left, int right, int& y)
{
    if (presetPanelMode == 0) return;
    root.AddLabel("preset/inputLabel", {left, y, left + 116, y + 34}, presetPanelMode == 1 ? "ポーズ名" : "ポーズを選択", true, 16);
    if (presetPanelMode == 1)
    {
        root.AddTextInput("preset/name", {left + 120, y, right, y + 34}, presetName, 127);
        y += 44;
        const int half = (left + right) / 2;
        root.AddButton("preset/confirmSave", {left, y, half - 8, y + 34}, "保存", keyed);
        root.AddButton("preset/cancel", {half + 8, y, right, y + 34}, "キャンセル");
    }
    else
    {
        root.AddDropdown("preset/list", {left + 120, y, right, y + 34}, presetNames, presetIndex);
        y += 44;
        const int column = (right - left - 16) / 3;
        const auto selected = motion.GetSelectedBodyParts();
        const bool anySelected = std::any_of(selected.begin(), selected.end(), [](bool value) { return value; });
        root.AddIconButton("preset/applyAll", {left, y, left + column, y + 34}, "全身に適用", "", keyed && !presetIds.empty(), false, false, 16);
        root.AddIconButton("preset/applySelected", {left + column + 8, y, left + column * 2 + 8, y + 34}, "選択部位のみ適用", "", keyed && anySelected && !presetIds.empty(), false, false, 16);
        root.AddIconButton("preset/cancel", {left + (column + 8) * 2, y, right, y + 34}, "キャンセル", "", true, false, false, 16);
    }
    y += 42;
    root.AddLabel("preset/result", {left, y, right, y + 24}, message, true, 14);
    y += 30;
}
