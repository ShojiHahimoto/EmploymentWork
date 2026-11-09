#pragma once

#include "Controller/CustomizeAttackEditorController.h"
#include "UI/UIRoot.h"

class CustomizePreviewController;
class Renderer;

/// <summary>技編集専用UI。表示・入力を担当し、データの確定と保存は既存Controllerへ委譲する。</summary>
class CustomizeAttackEditorView
{
public:
    /// <summary>別スロットへ入る際にフォーカスとスクロールを初期化する。</summary>
    void Reset();
    /// <summary>部品を登録し、当フレームの入力を一度だけ編集Controllerへ反映する。</summary>
    /// <param name="controller">技の下書き・保存を管理するController。</param>
    /// <param name="preview">再生状態を管理するController。</param>
    /// <param name="width">画面幅。</param>
    /// <param name="height">画面高さ。</param>
    /// <param name="status">既存画面と共有する保存結果。</param>
    /// <returns>モーション編集または戻る要求。</returns>
    CustomizeAttackEditorAction Update(CustomizeAttackEditorController& controller,
        CustomizePreviewController& preview, int width, int height, std::string& status);
    /// <summary>更新済みのUIだけを描画する。入力処理は行わない。</summary>
    /// <param name="renderer">専用UIの描画先。</param>
    void Draw(Renderer& renderer) const;
    /// <summary>白系キャンバスとプレビュー枠を3Dモデルより先に描画する。</summary>
    /// <param name="renderer">専用UIの描画先。</param>
    /// <param name="width">画面幅。</param>
    /// <param name="height">画面高さ。</param>
    void DrawBackdrop(Renderer& renderer, int width, int height) const;
    /// <summary>UIと重ならないプレビュー矩形を求める。</summary>
    /// <param name="width">画面幅。</param>
    /// <param name="height">画面高さ。</param>
    /// <returns>クライアント座標のプレビュー範囲。</returns>
    static RECT PreviewRegion(int width, int height);

private:
    /// <summary>見出しを追加し、次の行へ進む。</summary>
    /// <param name="text">見出し。</param>
    void Section(const char* text);
    /// <summary>ラベル付き行を確保する。</summary>
    /// <param name="id">部品識別子。</param>
    /// <param name="label">日本語ラベル。</param>
    /// <returns>右側の入力欄矩形。</returns>
    GameUI::Rect Row(const std::string& id, const char* label);
    /// <summary>整数入力行を追加する。</summary>
    /// <param name="id">部品識別子。</param>
    /// <param name="label">表示名。</param>
    /// <param name="value">編集値。</param>
    /// <param name="minimum">最小値。</param>
    /// <param name="maximum">最大値。</param>
    void Integer(const char* id, const char* label, int& value, int minimum, int maximum);
    /// <summary>基本情報、フレーム設定、硬直差の編集欄を登録する。</summary>
    /// <param name="category">通常地上・通常空中・必殺技の区別。</param>
    void BuildParameters(CustomizeAttackCategory category);
    /// <summary>技の種類、被弾時の挙動、必殺技コマンドを独立した欄へ登録する。</summary>
    /// <param name="category">編集中の技分類。</param>
    void BuildBehavior(CustomizeAttackCategory category);
    /// <summary>各攻撃判定の位置とサイズ、追加削除操作を登録する。</summary>
    void BuildHitboxes();
    /// <summary>内部0始まりのキャンセル設定を表示上1始まりで登録する。</summary>
    void BuildCancel();

    GameUI::Root root; // 入力捕捉と描画部品。Drawまで参照先を維持する。
    bool timelineDragging = false; // Timelineで押し始めたドラッグを領域外でも維持する。
    CustomizeAttackEditorEditState edit; // UI部品が参照する当フレームの編集スナップショット。
    int heightIndex = 0, usableIndex = 0, reactionIndex = 0, commandIndex = 0;
    int cancelStart = 1, cancelEnd = 1;
    bool normalCancel = false, specialCancel = false, jumpCancel = false;
    int scroll = 0, contentHeight = 1400, rowY = 0, panelWidth = 0;
    std::string message; // 専用画面に表示する日本語の保存結果。
};
