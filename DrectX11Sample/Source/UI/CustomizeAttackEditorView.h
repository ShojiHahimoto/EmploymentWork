#pragma once

#include "Controller/CustomizeAttackEditorController.h"
#include "UI/UIRoot.h"
#include "Data/MotionSkeletonDefinition.h"

class CustomizePreviewController;
class CustomizeMotionEditorController;
class Renderer;

/// <summary>技・モーション編集の共通専用UI。共通レイアウトと再生・Timelineを一度だけ実装する。</summary>
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
        CustomizePreviewController& preview, int width, int height, std::string& status,
        CustomizeMotionEditorController& motion, bool motionScreen = false, bool commonMotion = false,
        bool canUndo = false, bool canRedo = false);
	/// <summary>専用UIで継続中のデータ入力を履歴へ通知する。</summary>
	/// <returns>プリセット操作は単発、それ以外は入力欄の操作単位。</returns>
	GameUI::Root::EditInteraction GetEditInteraction() const
	{
		auto interaction = root.GetEditInteraction();
		return interaction.id.starts_with("text/preset/") ? GameUI::Root::EditInteraction{} : interaction;
	}
	/// <summary>履歴復元後に古い入力バッファとドラッグ捕捉だけを捨てる。</summary>
	void ResetAfterHistory();
	/// <summary>移動キー編集を選択中か取得する。関節IKとは別の編集対象。</summary>
	/// <returns>全身移動トラック選択時true。</returns>
	bool IsMovementSelected() const { return movementSelected; }
	/// <summary>関節マーカーと接続線を表示するか取得する。選択状態とは独立。</summary>
	/// <returns>マーカー表示が有効ならtrue。</returns>
	bool AreMarkersVisible() const { return showMarkers; }
	/// <summary>プレビュー内の専用UIを除外する矩形を取得する。</summary>
	/// <param name="width">画面幅。</param>
	/// <param name="height">画面高さ。</param>
	/// <returns>プレビュー上部ツールバー。</returns>
	RECT PreviewToolbar(int width, int height) const;
	/// <summary>当フレームに実際に描画したツールバー部品の範囲を取得する。</summary>
	/// <returns>非表示の行や部品間の空白を含まない除外範囲。</returns>
	const std::vector<RECT>& PreviewToolbarExclusions() const { return previewToolbarRects; }
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
	/// <summary>内部の英語ステータスを直接UIへ出さず、必要な通知だけ日本語へ変換する。</summary>
	/// <param name="status">Controllerの処理結果。</param>
	/// <returns>表示用通知。通常の内部進捗は空文字列。</returns>
	static std::string UserNotification(const std::string& status);
	std::vector<RECT> previewToolbarRects;
	bool suppressInputOnce = false; // 履歴復元直後の再構築では、同じ押下を二度処理しない。
	/// <summary>モーション画面固有の操作とプリセットダイアログを登録する。</summary>
	/// <param name="motion">編集対象。</param>
	/// <param name="preview">プレビュー。</param>
	/// <param name="common">汎用モーションか。</param>
	/// <param name="top">右パネル上端。</param>
	/// <param name="bottom">右パネル下端。</param>
	/// <param name="split">左右境界。</param>
	/// <param name="width">画面幅。</param>
	void BuildMotionControls(CustomizeMotionEditorController& motion, CustomizePreviewController& preview,
		bool common, int top, int bottom, int split, int width);
	/// <summary>専用UIのモーション操作結果だけをControllerへ渡す。</summary>
	/// <param name="motion">操作先。</param>
	/// <param name="preview">プレビュー。</param>
	/// <param name="common">汎用モーションか。</param>
	/// <param name="status">操作結果。</param>
	/// <param name="previewRegion">部位選択を継続できるプレビュー範囲。</param>
	void ApplyMotionControls(CustomizeMotionEditorController& motion, CustomizePreviewController& preview,
		bool common, std::string& status, const GameUI::Rect& previewRegion);
	/// <summary>プリセットボタン直下へ保存/読み込みの入力欄を登録する。</summary>
	/// <param name="motion">適用対象の選択状態。</param>
	/// <param name="keyed">現在フレームに姿勢キーがあるか。</param>
	/// <param name="left">内容領域左端。</param>
	/// <param name="right">内容領域右端。</param>
	/// <param name="y">追加先の行位置。登録した高さだけ進める。</param>
	void BuildInlinePosePreset(CustomizeMotionEditorController& motion, bool keyed, int left, int right, int& y);
	bool movementSelected = false;
	bool multipleSelectionEnabled = false;
	bool showMarkers = true; // 画面を行き来しても表示設定を維持する。
	bool motionScreenActive = false;
	int presetPanelMode = 0; // 0:閉、1:保存、2:読込。将来のモーションプリセットも同じ展開規則。
	GameUI::Rect presetPanelBounds{}; // スクロール補正後の表示範囲。フォーカス移動の判定に使う。
	int presetIndex = 0;
	int motionPanelScroll = 0;
	std::string motionName, presetName;
	std::vector<std::string> presetIds, presetNames;
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
