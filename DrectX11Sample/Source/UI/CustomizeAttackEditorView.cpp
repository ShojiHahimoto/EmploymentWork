#include "UI/CustomizeAttackEditorView.h"
#pragma execution_character_set("utf-8")
#include "Controller/CustomizePreviewController.h"
#include "UI/UIInputSystem.h"
#include "UI/UITheme.h"

#include <algorithm>
#include <limits>

namespace
{
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
    message.clear();
}

RECT CustomizeAttackEditorView::PreviewRegion(int width, int height)
{
    return { 0, 0, std::max(1, width / 2), std::max(1, height - 140) };
}

void CustomizeAttackEditorView::Section(const char* text)
{
    rowY += 12;
    root.AddSectionHeader(std::string("section/") + text,
        { 12, rowY, panelWidth - 12, rowY + 30 }, text, 20);
    rowY += 38;
}

GameUI::Rect CustomizeAttackEditorView::Row(const std::string& id, const char* label)
{
    const int split = panelWidth * 45 / 100;
    root.AddLabel(id + "/label", {12, rowY, split - 8, rowY + 32}, label, true, 16);
    GameUI::Rect result{ split, rowY, panelWidth - 20, rowY + 32 };
    rowY += 40;
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
    root.AddTextInput("name", Row("name", "技の名前"), data.displayName,
        CustomizeAttackEditorController::AttackNameBufferSize - 1);
    Integer("damage", "ダメージ", data.damage, 0, std::numeric_limits<int>::max());
    root.AddDropdown("height", Row("height", "攻撃属性"), {"上段", "中段", "下段"}, heightIndex);
    Section("フレーム設定");
    Integer("startup", "発生", data.frame.startup, 2, 100000);
    Integer("active", "持続", data.frame.active, 0, 100000);
    Integer("recovery", "硬直", data.frame.recovery, 0, 100000);
    Section("硬直差");
    Integer("hitAdvantage", "ヒット時", edit.hitFrameAdvantage, -100000, 100000);
    Integer("guardAdvantage", "ガード時", edit.guardFrameAdvantage, -100000, 100000);
    Section("攻撃の挙動");
    if (category == CustomizeAttackCategory::Special)
        root.AddDropdown("usable", Row("usable", "使用状態"), {"地上", "空中"}, usableIndex);
    else
        root.AddLabel("usable/fixed", Row("usable", "使用状態"),
            category == CustomizeAttackCategory::Air ? "空中（固定）" : "地上（固定）");
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
        root.AddLabel(id, {12, rowY, panelWidth - 120, rowY + 30}, "判定 " + std::to_string(i + 1));
        root.AddButton(id + "/delete", {panelWidth - 116, rowY, panelWidth - 20, rowY + 30},
            "削除", edit.attackData.hitboxes.size() > 1);
        rowY += 38;
        root.AddFloatInput(id + "/x", Row(id + "/x", "位置 横"), box.offset.x, -1.0e6f, 1.0e6f, .05f);
        root.AddFloatInput(id + "/y", Row(id + "/y", "位置 縦"), box.offset.y, -1.0e6f, 1.0e6f, .05f);
        root.AddFloatInput(id + "/w", Row(id + "/w", "大きさ 幅"), box.size.x, 0, 100, .05f);
        root.AddFloatInput(id + "/h", Row(id + "/h", "大きさ 高さ"), box.size.y, 0, 100, .05f);
    }
    root.AddButton("addBox", {12, rowY, panelWidth - 20, rowY + 32}, "＋ 攻撃判定を追加");
    rowY += 40;
}

void CustomizeAttackEditorView::BuildCancel()
{
    Section("キャンセル設定");
    root.AddCheckbox("cancel", {12, rowY, panelWidth - 20, rowY + 32},
        "攻撃キャンセルを許可", edit.attackData.canAttackCancel);
    rowY += 40;
    if (!edit.attackData.canAttackCancel) return;
    const int total = std::max(1, GetAttackTotalFrames(edit.attackData.frame));
    Integer("cancelStart", "開始フレーム", cancelStart, 1, total);
    Integer("cancelEnd", "終了フレーム", cancelEnd, 1, total);
    root.AddCheckbox("normalCancel", {12, rowY, panelWidth - 20, rowY + 32}, "通常攻撃", normalCancel);
    rowY += 36;
    root.AddCheckbox("specialCancel", {12, rowY, panelWidth - 20, rowY + 32}, "必殺技", specialCancel);
    rowY += 36;
    root.AddCheckbox("jumpCancel", {12, rowY, panelWidth - 20, rowY + 32}, "ジャンプ", jumpCancel);
    rowY += 40;
}

CustomizeAttackEditorAction CustomizeAttackEditorView::Update(CustomizeAttackEditorController& controller,
    CustomizePreviewController& preview, int width, int height, std::string& status)
{
    // 数値・日本語入力中の取消は編集欄へ渡す。次の取消、または戻るボタンで画面を戻す。
    const bool hadKeyboardCapture = root.WantsKeyboardCapture();
    const auto& input = GameUI::UIInputSystem::GetFrame();
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
    const RECT region = PreviewRegion(width, height);
    const int split = region.right, bottom = region.bottom;
    panelWidth = width - split;
    root.BeginFrame({0, 0, width, height}, input);
    root.AddPanel({split, 0, width, bottom});
    root.AddPanel({split + 8, 4, width - 8, 46}, GameUI::Theme::HeaderImage);
    root.AddImage("title/accent", {split + 12, 9, split + 32, 41}, GameUI::Theme::HeaderAccentImage);
    root.AddLabel("title", {split + 40, 8, width - 16, 44}, "技の編集", true, 24);
    rowY = 0;
    root.BeginScrollArea("parameters", {split, 50, width, std::max(51, bottom - 102)}, contentHeight, scroll);
    BuildParameters(controller.GetSelectedCategory());
    BuildHitboxes();
    BuildCancel();
    contentHeight = rowY + 16;
    root.EndScrollArea();
    root.AddIconButton("motion", {split + 16, bottom - 92, width - 16, bottom - 56},
        "モーションを編集", "motion");
    const int mid = split + panelWidth / 2;
    root.AddPrimaryButton("save", {split + 16, bottom - 48, mid - 4, bottom - 12}, "保存");
    root.AddIconButton("back", {mid + 4, bottom - 48, width - 16, bottom - 12}, "戻る", "back");

    root.AddPanel({0, bottom, width, height}, GameUI::Theme::PlaybackPanelImage);
    const int total = GetAttackTotalFrames(data.frame);
    root.AddLabel("frame", {16, bottom + 8, width / 2, bottom + 38},
        "現在フレーム  " + std::to_string(preview.GetCurrentFrame()) + " / " + std::to_string(total));
    const int actionFrame = preview.GetActionFrame();
    const char* phase = preview.GetCurrentFrame() == 0 ? "待機"
        : actionFrame < data.frame.startup - 1 ? "前隙"
        : actionFrame < GetAttackActiveEndFrameExclusive(data.frame) ? "持続" : "後隙";
    root.AddLabel("phase", {width / 2, bottom + 8, width - 16, bottom + 38}, phase);
    root.AddImageButton("previous", {16, bottom + 44, 72, bottom + 80}, "previous", "前へ");
    root.AddImageButton("play", {80, bottom + 44, 144, bottom + 80}, "play", "再生");
    root.AddImageButton("stop", {152, bottom + 44, 216, bottom + 80}, "pause", "停止");
    root.AddImageButton("next", {224, bottom + 44, 280, bottom + 80}, "next", "次へ");
    root.AddIconButton("camera", {300, bottom + 44, 484, bottom + 80},
        "カメラを初期化", "camera");
    root.AddLabel("status", {16, bottom + 90, width - 16, height - 8}, message, true, 16);
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
    controller.ApplyEditState(result);
    const int updatedTotal = GetAttackTotalFrames(controller.GetDraft().frame);
    preview.ClampCurrentFrame(updatedTotal);
    if (root.WasActivated("play")) preview.Play(updatedTotal);
    if (root.WasActivated("stop")) preview.Stop();
    if (root.WasActivated("previous")) preview.StepFrame(-1, updatedTotal);
    if (root.WasActivated("next")) preview.StepFrame(1, updatedTotal);
    if (root.WasActivated("camera")) preview.ResetCamera();
    if (root.WasActivated("save"))
    {
        status = controller.SaveDraft();
        message = status.starts_with("Saved:") ? "保存しました" : "保存に失敗しました";
    }
    if (root.WasActivated("motion")) return controller.RequestOpenMotionEditor();
    if (root.WasActivated("back")) return controller.RequestBack();
    if (!hadKeyboardCapture && !input.composing
        && (input.cancelPressed || input.keysPressed[static_cast<size_t>(GameUI::Key::Escape)]))
        return controller.RequestBack();
    return CustomizeAttackEditorAction::None;
}

void CustomizeAttackEditorView::Draw(Renderer& renderer) const
{
    root.Draw(renderer);
}
