#include "UI/CustomizeAttackEditorView.h"
#pragma execution_character_set("utf-8")
#include "Controller/CustomizePreviewController.h"
#include "Controller/CustomizeMotionEditorController.h"
#include "UI/UIInputSystem.h"
#include "UI/UITheme.h"
#include "UI/UIRenderer.h"
#include "System/Renderer.h"

#include <algorithm>
#include <limits>

namespace
{
    // この総フレーム数以上では数字だけ5刻みにする。目盛り線は各フレームに残す。
    constexpr int TimelineSparseLabelThreshold = 35;
    struct AttackEditorRegions
    {
        int split = 0;
        int previewBottom = 0;
        int timelineTop = 0;
        RECT previewPanel{};
        RECT preview{};
    };

    /// <summary>承認レイアウトの比率からプレビュー・操作欄・Timelineの境界を求める。</summary>
    AttackEditorRegions MakeRegions(int width, int height)
    {
        AttackEditorRegions result;
        result.split = std::max(1, width * 60 / 100);
        result.previewBottom = std::max(1, height * 68 / 100);
        result.timelineTop = std::max(result.previewBottom + 1, height * 77 / 100);
        result.previewPanel = {8, height * 8 / 100 + 4, std::max(9, result.split - 8), std::max(9, result.previewBottom - 8)};
        constexpr int inset = 10;
        result.preview = {
            result.previewPanel.left + inset, result.previewPanel.top + inset,
            std::max(result.previewPanel.left + inset + 1, result.previewPanel.right - inset),
            std::max(result.previewPanel.top + inset + 1, result.previewPanel.bottom - inset)};
        return result;
    }

    constexpr AttackHeight Heights[] = { AttackHeight::High, AttackHeight::Mid, AttackHeight::Low };
    constexpr HitReactionType Reactions[] = { HitReactionType::Normal, HitReactionType::Down,
        HitReactionType::Burst, HitReactionType::HardBurst };
    constexpr AttackCommandId Commands[] = { AttackCommandId::None, AttackCommandId::Hadouken,
        AttackCommandId::Shoryuu, AttackCommandId::Yoga, AttackCommandId::ReverseYoga, AttackCommandId::FullRotate };

    /// <summary>列挙値を表示用番号へ変換する。</summary>
    /// <param name="values">表示順の値。</param>
    /// <param name="value">探す値。</param>
    /// <returns>一致する番号。未知の値は先頭。</returns>
    template<class T, size_t N> int IndexOf(const T (&values)[N], const T& value)
    {
        const auto found = std::find(std::begin(values), std::end(values), value);
        return found == std::end(values) ? 0 : static_cast<int>(found - std::begin(values));
    }
}

void CustomizeAttackEditorView::Reset()
{
    root = GameUI::Root{};
    scroll = 0;
    timelineDragging = false;
    movementSelected = false;
    presetPanelMode = 0;
    presetName.clear();
    presetPanelBounds = {};
    message.clear();
    previewToolbarRects.clear();
}

std::string CustomizeAttackEditorView::UserNotification(const std::string& status)
{
    if (status.empty()) return {};
    if (status.starts_with("Saved pose preset:")) return "ポーズプリセットを保存しました。";
    if (status.starts_with("Applied pose preset:")) return "ポーズプリセットを適用しました。";
    if (status == "Preset name is empty.") return "プリセット名を入力してください。";
    if (status == "Preset display name already exists.") return "同じ名前のプリセットが存在します。";
    if (status.find("failed") != std::string::npos || status.find("Failed") != std::string::npos)
        return "処理に失敗しました。内容を確認して再度お試しください。";
    if (status.find("requires") != std::string::npos || status.starts_with("Add a "))
        return "編集するフレームにキーフレームを追加してください。";
    // 日本語で用意した利用者向け通知だけはそのまま表示する。読み込み進捗等の英語は隠す。
    if (static_cast<unsigned char>(status.front()) >= 0x80) return status;
    return {};
}

RECT CustomizeAttackEditorView::PreviewRegion(int width, int height)
{
    return MakeRegions(width, height).preview;
}

void CustomizeAttackEditorView::DrawBackdrop(Renderer& renderer, int width, int height) const
{
    const AttackEditorRegions regions = MakeRegions(width, height);
    Renderer::DrawScreenRect({0, 0, width, height}, DirectX::SimpleMath::Color(0.965f, 0.972f, 0.978f, 1.0f));

    // 3D描画の前に枠を置き、あとから描くモデルが枠の内側を使う。
    GameUI::Root backdrop;
    backdrop.BeginFrame({0, 0, width, height}, {});
    backdrop.AddPanel({regions.previewPanel.left, regions.previewPanel.top,
        regions.previewPanel.right, regions.previewPanel.bottom}, GameUI::Theme::PanelImage);
    backdrop.EndFrame();
    backdrop.Draw(renderer);
}

void CustomizeAttackEditorView::Section(const char* text)
{
    if (rowY > 0) root.AddRule({12, rowY + 2, panelWidth - 12, rowY + 3});
    rowY += 6;
    root.AddImage(std::string("section/") + text + "/accent", {12, rowY + 4, 19, rowY + 24},
        GameUI::Theme::SectionAccentImage);
    root.AddLabel(std::string("section/") + text, {26, rowY, panelWidth - 12, rowY + 28}, text, true, 20, false, true);
    rowY += 32;
}

GameUI::Rect CustomizeAttackEditorView::Row(const std::string& id, const char* label)
{
    const int split = panelWidth * 25 / 100;
    root.AddLabel(id + "/label", {12, rowY, split - 8, rowY + 32}, label, true, 16);
    GameUI::Rect result{ split, rowY, panelWidth - 20, rowY + 32 };
    rowY += 36;
    return result;
}

void CustomizeAttackEditorView::Integer(const char* id, const char* label, int& value, int minimum, int maximum)
{
    root.AddIntegerInput(id, Row(id, label), value, minimum, maximum);
}

void CustomizeAttackEditorView::BuildParameters(CustomizeAttackCategory category)
{
    auto& data = edit.attackData;
    Section("基本設定");
    root.AddTextInput("name", Row("name", "技名"), data.displayName,
        CustomizeAttackEditorController::AttackNameBufferSize - 1);
    const int half = panelWidth / 2;
    root.AddLabel("damage/label", {20, rowY, 114, rowY + 32}, "ダメージ", true, 16);
    root.AddIntegerInput("damage", {114, rowY, half - 8, rowY + 32}, data.damage, 0, std::numeric_limits<int>::max());
    root.AddLabel("height/label", {half + 8, rowY, half + 104, rowY + 32}, "攻撃属性", true, 16);
    root.AddDropdown("height", {half + 104, rowY, panelWidth - 16, rowY + 32}, {"上段", "中段", "下段"}, heightIndex);
    rowY += 36;
    Section("フレーム設定");
    {
        const int gap = 8;
        const int columnWidth = std::max(1, (panelWidth - 24 - gap * 2) / 3);
        const int columnX[] = {12, 12 + columnWidth + gap, 12 + (columnWidth + gap) * 2};
        const char* labels[] = {"発生", "持続", "硬直"};
        const char* ids[] = {"startup", "active", "recovery"};
        int* values[] = {&data.frame.startup, &data.frame.active, &data.frame.recovery};
        const int minimums[] = {2, 0, 0};
        for (int i = 0; i < 3; ++i)
        {
            root.AddLabel(std::string("frame/") + ids[i] + "/label",
                {columnX[i], rowY, columnX[i] + 48, rowY + 32}, labels[i], true, 16);
            root.AddIntegerInput(ids[i], {columnX[i] + 48, rowY, columnX[i] + columnWidth, rowY + 32},
                *values[i], minimums[i], 100000);
        }
        rowY += 36;
    }
    Section("硬直差");
    {
        const int gap = 12;
        const int columnWidth = std::max(1, (panelWidth - 24 - gap) / 2);
        root.AddLabel("hitAdvantage/label", {12, rowY, 104, rowY + 32}, "ヒット時", true, 16);
        root.AddIntegerInput("hitAdvantage", {104, rowY, 12 + columnWidth, rowY + 32},
            edit.hitFrameAdvantage, -100000, 100000);
        const int secondX = 12 + columnWidth + gap;
        root.AddLabel("guardAdvantage/label", {secondX, rowY, secondX + 92, rowY + 32}, "ガード時", true, 16);
        root.AddIntegerInput("guardAdvantage", {secondX + 92, rowY, secondX + columnWidth, rowY + 32},
            edit.guardFrameAdvantage, -100000, 100000);
        rowY += 36;
    }
}

void CustomizeAttackEditorView::BuildBehavior(CustomizeAttackCategory category)
{
    auto& data = edit.attackData;
    Section("攻撃の挙動");
    if (category == CustomizeAttackCategory::Special)
        root.AddDropdown("usable", Row("usable", "技の種類"), {"地上技", "空中技"}, usableIndex);
    else
        root.AddLabel("usable/fixed", Row("usable", "技の種類"),
            category == CustomizeAttackCategory::Air ? "空中技（固定）" : "地上技（固定）");
    if (data.usableState == AttackUsableState::Air)
        root.AddLabel("reaction/fixed", Row("reaction", "被弾の挙動"), "ノーマル（固定）", true, 16);
    else
        root.AddDropdown("reaction", Row("reaction", "被弾の挙動"),
            {"ノーマル", "ダウン", "バースト", "ハードバースト"}, reactionIndex);
    if (category == CustomizeAttackCategory::Special)
        root.AddDropdown("command", Row("command", "コマンド"),
            {"なし", "波動", "昇竜", "ヨガ", "逆ヨガ", "一回転"}, commandIndex);
}

void CustomizeAttackEditorView::BuildHitboxes()
{
    Section("攻撃判定");
    for (size_t i = 0; i < edit.attackData.hitboxes.size(); ++i)
    {
        const std::string id = "box/" + std::to_string(i);
        auto& box = edit.attackData.hitboxes[i];
        if (edit.attackData.hitboxes.size() > 1)
        {
            root.AddLabel(id, {12, rowY, panelWidth - 120, rowY + 28}, "判定 " + std::to_string(i + 1));
            root.AddButton(id + "/delete", {panelWidth - 96, rowY, panelWidth - 16, rowY + 28}, "削除");
            rowY += 32;
        }
        for (int axisRow = 0; axisRow < 2; ++axisRow)
        {
            const bool position = axisRow == 0;
            const int labelEnd = panelWidth * 24 / 100;
            const int middle = panelWidth * 60 / 100;
            root.AddNineSliceImage(id + "/row/" + std::to_string(axisRow), {20, rowY, labelEnd, rowY + 30},
                GameUI::Theme::LabelBackgroundImage);
            root.AddLabel(id + "/label/" + std::to_string(axisRow), {24, rowY, labelEnd, rowY + 30}, position ? "位置" : "大きさ", true, 16);
            root.AddLabel(id + "/horizontal/" + std::to_string(axisRow), {labelEnd + 12, rowY, labelEnd + 50, rowY + 30}, "横", true, 16);
            root.AddFloatInput(id + (position ? "/x" : "/w"), {labelEnd + 50, rowY, middle - 8, rowY + 30},
                position ? box.offset.x : box.size.x, position ? -1.0e6f : 0.0f, position ? 1.0e6f : 100.0f, .05f);
            root.AddLabel(id + "/vertical/" + std::to_string(axisRow), {middle + 8, rowY, middle + 46, rowY + 30}, "縦", true, 16);
            root.AddFloatInput(id + (position ? "/y" : "/h"), {middle + 46, rowY, panelWidth - 16, rowY + 30},
                position ? box.offset.y : box.size.y, position ? -1.0e6f : 0.0f, position ? 1.0e6f : 100.0f, .05f);
            rowY += 34;
        }
    }
    root.AddButton("addBox", {12, rowY, panelWidth - 20, rowY + 30}, "＋ 攻撃判定を追加");
    rowY += 34;
}

void CustomizeAttackEditorView::BuildCancel()
{
    Section("キャンセル設定");
    const int half = panelWidth / 2;
    root.AddCheckbox("cancel", {12, rowY, half, rowY + 32},
        "キャンセルを有効にする", edit.attackData.canAttackCancel);
    if (!edit.attackData.canAttackCancel) { rowY += 36; return; }
    const int total = std::max(1, GetAttackTotalFrames(edit.attackData.frame));
    const int endX = panelWidth * 74 / 100;
    root.AddLabel("cancelStart/label", {half - 4, rowY, half + 36, rowY + 32}, "開始", true, 16);
    root.AddIntegerInput("cancelStart", {half + 36, rowY, endX - 8, rowY + 32}, cancelStart, 1, total);
    root.AddLabel("cancelEnd/label", {endX - 4, rowY, endX + 36, rowY + 32}, "終了", true, 16);
    root.AddIntegerInput("cancelEnd", {endX + 36, rowY, panelWidth - 16, rowY + 32}, cancelEnd, 1, total);
    rowY += 36;
    const int column = (panelWidth - 24) / 3;
    root.AddCheckbox("normalCancel", {12, rowY, 12 + column, rowY + 30}, "通常技", normalCancel);
    root.AddCheckbox("specialCancel", {12 + column, rowY, 12 + column * 2, rowY + 30}, "必殺技", specialCancel);
    root.AddCheckbox("jumpCancel", {12 + column * 2, rowY, panelWidth - 12, rowY + 30}, "ジャンプ", jumpCancel);
    rowY += 34;
}

CustomizeAttackEditorAction CustomizeAttackEditorView::Update(CustomizeAttackEditorController& controller,
    CustomizePreviewController& preview, int width, int height, std::string& status,
    CustomizeMotionEditorController& motion, bool motionScreen, bool commonMotion, bool canUndo, bool canRedo)
{
    const bool canCommitTextForUndo = root.WantsTextInput();
    motionScreenActive = motionScreen;
    previewToolbarRects.clear();
    message = UserNotification(status);
    const bool hadPresetPanel = presetPanelMode != 0;
    multipleSelectionEnabled = motion.IsMultipleSelectionEnabled();
    motionName = motion.GetDisplayNameBuffer().data();
    // 数値・日本語入力中の取消は編集欄へ渡す。次の取消、または戻るボタンで画面を戻す。
    const bool hadKeyboardCapture = root.WantsKeyboardCapture();
    const GameUI::InputFrame input = suppressInputOnce ? GameUI::InputFrame{} : GameUI::UIInputSystem::GetFrame();
    suppressInputOnce = false;
    edit = controller.GetEditState();
    auto& data = edit.attackData;
    heightIndex = IndexOf(Heights, data.attackHeight);
    reactionIndex = IndexOf(Reactions, data.hitReactionType);
    commandIndex = IndexOf(Commands, data.commandId);
    usableIndex = data.usableState == AttackUsableState::Air ? 1 : 0;
    cancelStart = data.cancelSetting.startFrame + 1;
    cancelEnd = data.cancelSetting.endFrame + 1;
    const auto& types = data.cancelSetting.cancelTypes;
    normalCancel = std::find(types.begin(), types.end(), AttackCancelType::Normal) != types.end();
    specialCancel = std::find(types.begin(), types.end(), AttackCancelType::Special) != types.end();
    jumpCancel = std::find(types.begin(), types.end(), AttackCancelType::Jump) != types.end();
    const bool hadCancel = data.canAttackCancel;
    const AttackEditorRegions regions = MakeRegions(width, height);
    const int split = regions.split;
    const int bottom = regions.timelineTop;
    panelWidth = width - split - 8;
    root.BeginFrame({0, 0, width, height}, input);
    const int headerHeight = std::max(44, height * 8 / 100);
    root.AddPanel({0, 0, width, headerHeight}, GameUI::Theme::HeaderImage);
    root.AddIconButton("back", {16, 8, 82, headerHeight - 10}, "", "back");
    root.AddImage("title/accent", {94, 0, 128, headerHeight}, GameUI::Theme::HeaderAccentImage);
    root.AddLabel("title", {136, 6, width * 28 / 100, headerHeight - 6}, "技の編集", true, 34.0f, false, true);
    const int nameX = width * 28 / 100;
    root.AddLabel("header/name/label", {nameX, 8, nameX + 58, headerHeight - 10}, commonMotion ? "名前" : "技名", true, 20);
    root.AddTextInput("header/name", {nameX + 62, 10, width * 56 / 100, headerHeight - 12}, commonMotion ? motionName : data.displayName,
        CustomizeAttackEditorController::AttackNameBufferSize - 1, true, true);
    if (!commonMotion)
    {
    root.AddLabel("header/category/label", {width * 58 / 100, 8, width * 62 / 100, headerHeight - 10}, "分類", true, 20);
    root.AddPanel({width * 63 / 100, 10, width * 75 / 100, headerHeight - 12}, GameUI::Theme::ButtonImage);
    root.AddLabel("header/category", {width * 63 / 100 + 10, 10, width * 75 / 100, headerHeight - 12},
        controller.GetSelectedCategory() == CustomizeAttackCategory::Special ? "必殺技" :
        controller.GetSelectedCategory() == CustomizeAttackCategory::Air ? "空中技" : "地上技", true, 20);
    }
    root.AddIconButton("undo", {width * 78 / 100, 8, width * 83 / 100, headerHeight - 10}, "", "undo", canUndo || canCommitTextForUndo, false);
    root.AddIconButton("redo", {width * 84 / 100, 8, width * 89 / 100 - 8, headerHeight - 10}, "", "redo", canRedo, false);
    root.AddPrimaryButton("save", {width * 89 / 100, 8, width - 16, headerHeight - 10}, "保存");
    root.AddPanel({split, headerHeight + 4, width - 8, bottom - 8}, GameUI::Theme::PanelImage);
    rowY = 0;
    const int actionTop = bottom - 64;
    if (motionScreen)
    {
        BuildMotionControls(motion, preview, commonMotion, headerHeight + 4, bottom - 8, split, width);
        const RECT toolbar = PreviewToolbar(width, height);
        const int x = toolbar.left, y = toolbar.top;
        previewToolbarRects.push_back(toolbar);
        root.AddPanel({x, y, toolbar.right, toolbar.bottom}, GameUI::Theme::PlaybackPanelImage);
        root.AddIconButton("gizmo/rotate", {x + 4, y + 4, x + 82, toolbar.bottom - 4}, "回転", "rotate", true, false, !movementSelected && !preview.translationGizmoEnabled, 16);
        root.AddIconButton("gizmo/move", {x + 86, y + 4, x + 164, toolbar.bottom - 4}, "移動", "translate", true, false, movementSelected || preview.translationGizmoEnabled, 16);
        root.AddCheckbox("selection/multiple", {x + 170, y + 4, x + 274, toolbar.bottom - 4}, "複数選択", multipleSelectionEnabled, true, 16);
        root.AddCheckbox("preview/markers", {x + 280, y + 4, toolbar.right - 4, toolbar.bottom - 4}, "マーカー", showMarkers, true, 16);
        if (multipleSelectionEnabled)
        {
            const RECT all{x + 100, toolbar.bottom + 6, x + 206, toolbar.bottom + 36};
            const RECT none{x + 212, toolbar.bottom + 6, x + 318, toolbar.bottom + 36};
            previewToolbarRects.push_back(all);
            previewToolbarRects.push_back(none);
            root.AddIconButton("select/all", {all.left, all.top, all.right, all.bottom}, "全選択", "select_all", true, false, false, 16);
            root.AddIconButton("select/none", {none.left, none.top, none.right, none.bottom}, "全解除", "deselect_all", true, false, false, 16);
        }
    }
    else
    {
    root.BeginScrollArea("parameters", {split + 8, headerHeight + 12, width - 8,
        std::max(headerHeight + 13, actionTop - 34)}, contentHeight, scroll);
    BuildParameters(controller.GetSelectedCategory());
    BuildBehavior(controller.GetSelectedCategory());
    BuildHitboxes();
    BuildCancel();
    contentHeight = rowY + 16;
    root.EndScrollArea();
    root.AddIconButton("motion", {split + panelWidth / 5, actionTop, width - panelWidth / 5, bottom - 22},
        "モーションを編集", "motion");
    }

    root.AddPanel({8, regions.previewBottom, split - 8, bottom - 8}, GameUI::Theme::PlaybackPanelImage);
    const int total = commonMotion ? std::max(1, motion.GetDraft().totalFrames) : GetAttackTotalFrames(data.frame);
    const int controlsY = regions.previewBottom + 10;
    const int controlsBottom = bottom - 18;
    const int buttonWidth = (split * 46 / 100 - 36) / 5;
    const char* buttonIds[] = {"play", "stop", "previous", "next"};
    const char* buttonImages[] = {"play", "pause", "previous", "next"};
    const char* buttonLabels[] = {"再生", "停止", "先頭へ", "末尾へ"};
    for (int i = 0; i < 4; ++i)
        root.AddImageButton(buttonIds[i], {20 + i * (buttonWidth + 8), controlsY,
            20 + i * (buttonWidth + 8) + buttonWidth, controlsBottom}, buttonImages[i], buttonLabels[i], true, false);
    const int frameX = split * 50 / 100;
    root.AddRule({frameX - 8, controlsY + 6, frameX - 7, controlsBottom - 6});
    root.AddLabel("frame", {frameX, controlsY, split * 73 / 100, controlsBottom},
        "現在 " + std::to_string(preview.GetCurrentFrame()) + " / " + std::to_string(total) + " フレーム", true, 18);
    root.AddIconButton("camera", {split * 74 / 100, controlsY, split - 20, controlsBottom}, "カメラリセット", "camera", true, false);

    const GameUI::Rect timeline{8, bottom + 4, width - 8, height - 4};
    root.AddPanel(timeline, GameUI::Theme::PlaybackPanelImage);

    const int trackLeft = 138;
    const int trackRight = width - 40;
    const int trackWidth = std::max(1, trackRight - trackLeft);
    // 押下直後から反映し、ドラッグ中は画面外でも端へクランプする。
    // 再生と手動シークが競合しないよう、操作開始時に再生を止める。
    if (input.focusLost) timelineDragging = false;
    if (!input.focusLost && input.pointerPressed && timeline.Contains(input.pointerPosition) && input.pointerPosition.x >= trackLeft)
    {
        timelineDragging = true;
        preview.Stop();
    }
    if (timelineDragging)
    {
        const double fraction = static_cast<double>(std::clamp<long>(input.pointerPosition.x - trackLeft, 0, trackWidth)) / trackWidth;
        preview.SetCurrentFrame(static_cast<int>(fraction * total + 0.5));
        if (input.pointerReleased || !input.pointerDown) timelineDragging = false;
    }
    const int trackHeight = std::max(24, (height - bottom) * 23 / 100);
    const int trackTop = height - trackHeight - 12;
    const int startupFrames = std::max(0, data.frame.startup - 1);
    const int activeFrames = std::max(0, data.frame.active);
    const int recoveryFrames = std::max(0, data.frame.recovery);
    // 0Fの待機姿勢は内部仕様のまま。完成画像にない待機用の帯は作らない。
    const int phaseTotal = std::max(1, startupFrames + activeFrames + recoveryFrames);
    const int startupWidth = static_cast<int>(static_cast<long long>(trackWidth) * startupFrames / phaseTotal);
    const int activeWidth = static_cast<int>(static_cast<long long>(trackWidth) * activeFrames / phaseTotal);
    const int recoveryWidth = trackWidth - startupWidth - activeWidth;
    int phaseX = trackLeft;
    if (startupWidth > 0)
    {
        root.AddNineSliceImage("timeline/startup", {phaseX, trackTop, phaseX + startupWidth, trackTop + trackHeight},
            "phase_startup");
        if (startupWidth > 56) root.AddLabel("timeline/startup/label",
            {phaseX + 4, trackTop + 2, phaseX + startupWidth - 2, trackTop + trackHeight - 2},
            "前隙（" + std::to_string(startupFrames) + "F）", true, 19, true);
        phaseX += startupWidth;
    }
    if (activeWidth > 0)
    {
        root.AddNineSliceImage("timeline/active", {phaseX, trackTop, phaseX + activeWidth, trackTop + trackHeight},
            "phase_active");
        if (activeWidth > 56) root.AddLabel("timeline/active/label",
            {phaseX + 2, trackTop + 2, phaseX + activeWidth - 2, trackTop + trackHeight - 2},
            "持続（" + std::to_string(activeFrames) + "F）", true, 19, true);
        phaseX += activeWidth;
    }
    if (recoveryWidth > 0)
    {
        root.AddNineSliceImage("timeline/recovery", {phaseX, trackTop, trackRight, trackTop + trackHeight},
            "phase_recovery");
        if (recoveryWidth > 56) root.AddLabel("timeline/recovery/label",
            {phaseX + 2, trackTop + 2, trackRight - 2, trackTop + trackHeight - 2},
            "後隙（" + std::to_string(recoveryFrames) + "F）", true, 19, true);
    }
    const int divisions = std::max(1, total);
    const int labelStep = total >= TimelineSparseLabelThreshold ? 5 : 1;
    const int rulerY = bottom + (height - bottom) * 25 / 100;
    const int playheadX = trackLeft + trackWidth * std::clamp(preview.GetCurrentFrame(), 0, total) / std::max(1, total);
    root.AddRule({trackLeft, rulerY, trackRight, rulerY + 1});
    const int poseY = rulerY + 18;
    const int moveY = poseY + 36;
    const auto trackLabel = [&](const char* id, const char* label, const char* image, int y)
    {
        root.AddPanel({18, y - 15, 126, y + 15}, GameUI::Theme::ButtonImage);
        root.AddImage(std::string(id) + "/icon", {26, y - 10, 46, y + 10}, image);
        root.AddLabel(id, {50, y - 14, 122, y + 14}, label, true, 16, true, true);
    };
    trackLabel("timeline/pose", "ポーズ", "motion", poseY);
    trackLabel("timeline/movement", "移動", "translate", moveY);
    root.AddRule({trackLeft, poseY, trackRight, poseY + 3});
    root.AddRule({trackLeft, moveY, trackRight, moveY + 3});
    // 表示フレームは内部キー+1。0は待機姿勢なのでキーを表示しない。
    for (int frame = 0; frame < total; ++frame)
    {
        const int x = trackLeft + static_cast<int>(static_cast<long long>(trackWidth) * (frame + 1) / std::max(1, total));
        const bool pose = motion.HasMotionKeyframe(frame);
        const bool move = commonMotion ? motion.HasRootOffsetKeyframe(frame) : motion.HasAttackMovementKeyframe(data, frame);
        for (int lane = 0; lane < 2; ++lane)
        {
            if (lane == 0 ? !pose : !move) continue;
            const int y = lane == 0 ? poseY : moveY;
            root.AddImage("key/" + std::to_string(lane) + "/" + std::to_string(frame), {x - 9, y - 9, x + 9, y + 9}, lane == 0 ? "keyframe" : "keyframe_movement");
            if (input.pointerPressed && std::abs(input.pointerPosition.x - x) <= 10 && std::abs(input.pointerPosition.y - y) <= 10)
            {
                preview.Stop(); preview.SetCurrentFrame(frame + 1); movementSelected = lane == 1;
                timelineDragging = false;
                motion.EndIkEdit(preview);
                motion.RefreshFrameEditValues(frame);
            }
        }
    }
    for (int i = 0; i <= divisions; ++i)
    {
        const int x = trackLeft + static_cast<int>(static_cast<long long>(trackWidth) * i / divisions);
        const int frame = total * i / divisions;
        root.AddRule({x, rulerY - 10, x + 1, rulerY});
        // 現在位置の数字は緑の目印内に一度だけ描く。近接する目盛り文字も重ねない。
        const bool endpoint = frame == 0 || frame == total;
        const bool nearLastLabel = frame != total && total - frame < labelStep;
        if ((endpoint || (frame % labelStep == 0 && !nearLastLabel)) && std::abs(x - playheadX) >= 28)
            root.AddLabel("timeline/tick/" + std::to_string(i), {x - 22, rulerY - 38,
                x + 22, rulerY - 14}, std::to_string(frame), true, 15, true);
    }
    root.AddRule({playheadX, rulerY - 24, playheadX + 2, trackTop});
    root.AddImage("timeline/playhead", {playheadX - 20, rulerY - 39, playheadX + 20, rulerY - 9},
        "playhead");
    root.AddLabel("timeline/playhead/value", {playheadX - 20, rulerY - 38, playheadX + 20, rulerY - 14},
        std::to_string(preview.GetCurrentFrame()), true, 15, true);
    if (!motionScreen) root.AddLabel("status", {split + 24, bottom - 92, width - 24, bottom - 66}, message, true, 13);
    root.EndFrame();

    // UIの選択番号は保存しない。既存Controllerと同じ列挙値・内部0始まりへ戻す。
    data.attackHeight = Heights[heightIndex];
    data.usableState = usableIndex == 1 ? AttackUsableState::Air : AttackUsableState::Ground;
    data.hitReactionType = Reactions[reactionIndex];
    data.commandId = Commands[commandIndex];
    if (hadCancel)
    {
        data.cancelSetting.startFrame = cancelStart - 1;
        data.cancelSetting.endFrame = std::max(cancelStart, cancelEnd) - 1;
        data.cancelSetting.cancelTypes.clear();
        if (normalCancel) data.cancelSetting.cancelTypes.push_back(AttackCancelType::Normal);
        if (specialCancel) data.cancelSetting.cancelTypes.push_back(AttackCancelType::Special);
        if (jumpCancel) data.cancelSetting.cancelTypes.push_back(AttackCancelType::Jump);
    }
    if (!hadCancel && data.canAttackCancel && data.cancelSetting.startFrame == 0
        && data.cancelSetting.endFrame == 0 && data.cancelSetting.cancelTypes.empty())
    {
        data.cancelSetting.startFrame = GetAttackActiveEndFrameExclusive(data.frame);
        data.cancelSetting.endFrame = std::max(data.cancelSetting.startFrame, GetAttackTotalFrames(data.frame) - 1);
        data.cancelSetting.cancelTypes.push_back(AttackCancelType::Special);
    }

    // 部品はDrawまでスナップショットを参照する。配列の追加削除は別コピーへ適用して参照切れを防ぐ。
    auto result = edit;
    for (size_t i = 0; i < data.hitboxes.size(); ++i)
        if (root.WasActivated("box/" + std::to_string(i) + "/delete") && result.attackData.hitboxes.size() > 1)
        {
            result.attackData.hitboxes.erase(result.attackData.hitboxes.begin() + static_cast<std::ptrdiff_t>(i));
            break;
        }
    if (root.WasActivated("addBox"))
    {
        AttackHitboxData box;
        box.offset = {2.5f, 4.0f};
        box.size = {2.0f, 2.0f};
        result.attackData.hitboxes.push_back(box);
    }
    if (motionScreen)
    {
        edit = result;
        ApplyMotionControls(motion, preview, commonMotion, status,
            {regions.preview.left, regions.preview.top, regions.preview.right, regions.preview.bottom});
        result = edit;
    }
    if (!commonMotion) controller.ApplyEditState(result);
    const int updatedTotal = commonMotion ? std::max(1, motion.GetDraft().totalFrames) : GetAttackTotalFrames(controller.GetDraft().frame);
    if (!commonMotion && motion.HasDraft())
    {
        motion.GetDraft().totalFrames = updatedTotal;
        motion.GetDraft().looping = false;
    }
    preview.ClampCurrentFrame(updatedTotal);
    if (root.WasActivated("play")) preview.Play(updatedTotal);
    if (root.WasActivated("stop")) preview.Stop();
    if (root.WasActivated("previous")) { preview.Stop(); preview.SetCurrentFrame(0); }
    if (root.WasActivated("next")) { preview.Stop(); preview.SetCurrentFrame(updatedTotal); }
    if (root.WasActivated("camera")) preview.ResetCamera();
    if (root.WasActivated("save"))
    {
        return CustomizeAttackEditorAction::Save;
    }
    if (hadPresetPanel && !input.composing && (input.cancelPressed || input.keysPressed[static_cast<size_t>(GameUI::Key::Escape)])) presetPanelMode = 0;
    if (root.WasActivated("undo")) return CustomizeAttackEditorAction::Undo;
    if (root.WasActivated("redo")) return CustomizeAttackEditorAction::Redo;
    if (root.WasActivated("motion")) return motionScreen ? (commonMotion ? CustomizeAttackEditorAction::Back : CustomizeAttackEditorAction::OpenAttackEditor) : controller.RequestOpenMotionEditor();
    if (root.WasActivated("back")) return controller.RequestBack();
    if (!hadPresetPanel && presetPanelMode == 0 && !hadKeyboardCapture && !input.composing
        && (input.cancelPressed || input.keysPressed[static_cast<size_t>(GameUI::Key::Escape)]))
        return controller.RequestBack();
    return CustomizeAttackEditorAction::None;
}

void CustomizeAttackEditorView::Draw(Renderer& renderer) const
{
    root.Draw(renderer);
}

void CustomizeAttackEditorView::ResetAfterHistory()
{
    root = GameUI::Root{};
    presetPanelMode = 0;
    timelineDragging = false;
    suppressInputOnce = true;
    // スクロール・マーカー表示・編集トラックは利用者の操作状態として維持する。
}
