#pragma once

#include "Data/AttackData.h"
#include "Controller/CustomizeTypes.h"

#include <array>
#include <string>

enum class CustomizeAttackEditorAction
{
	None,
	OpenMotionEditor,
	Back
};

/// <summary>
/// カスタマイズ画面の AttackData 下書き、保存、スロット概要を管理する。
/// </summary>
class CustomizeAttackEditorController
{
public:
	static constexpr int GroundAttackSlotCount = 20;
	static constexpr int AirAttackSlotCount = 20;
	static constexpr int SpecialAttackSlotCount = 20;
	static constexpr int MaxAttackSlotCount = 20;
	static constexpr int AttackNameBufferSize = 128;
	static constexpr int MotionDataIdBufferSize = 128;

	/// <summary>
	/// 指定カテゴリとスロット番号の技データを編集対象として読み込む。
	/// </summary>
	/// <param name="category">編集する技カテゴリ。</param>
	/// <param name="slotIndex">カテゴリ内スロット番号。</param>
	/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
	std::string SelectSlot(CustomizeAttackCategory category, int slotIndex);

	/// <summary>
	/// 編集中の draft を JSON として保存する。
	/// </summary>
	/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
	std::string SaveDraft();

	/// <summary>
	/// AttackData の編集 UI と、保存/遷移ボタンの入力処理を実行する。
	/// </summary>
	/// <param name="previewTotalFrames">プレビュー表示上の総フレーム数。</param>
	/// <param name="statusMessage">画面に表示する処理結果メッセージ。</param>
	/// <returns>Scene 側で扱う必要がある遷移要求。</returns>
	CustomizeAttackEditorAction DrawEditorControls(int previewTotalFrames, std::string& statusMessage);

	/// <summary>
	/// ImGui 入力欄の内容を draft に反映する。
	/// </summary>
	void SyncDraftFromEditor();

	/// <summary>
	/// 編集中の技に、スロット単位で一意になる MotionData ID を割り当てる。
	/// </summary>
	void EnsureDraftMotionDataId();

	/// <summary>
	/// 選択カテゴリのスロット JSON を確認し、一覧表示用の保存済み名を更新する。
	/// </summary>
	/// <param name="category">更新する技カテゴリ。</param>
	void RefreshSlotSummaries(CustomizeAttackCategory category);

	/// <summary>
	/// スロット番号と保存済み技名をまとめた、ImGui Button 用ラベルを作る。
	/// </summary>
	/// <param name="category">表示する技カテゴリ。</param>
	/// <param name="slotIndex">カテゴリ内スロット番号。</param>
	/// <returns>表示名と ImGui ID を含むボタンラベル。</returns>
	std::string BuildSlotButtonLabel(CustomizeAttackCategory category, int slotIndex) const;

	/// <summary>
	/// カテゴリとスロット番号から、assets/AttackData 配下の保存 ID を作る。
	/// </summary>
	/// <param name="category">保存カテゴリ。</param>
	/// <param name="slotIndex">カテゴリ内スロット番号。</param>
	/// <returns>拡張子なしの AttackData ID。</returns>
	std::string BuildAttackDataId(CustomizeAttackCategory category, int slotIndex) const;

	/// <summary>
	/// カテゴリとスロット番号から、assets/MotionData 配下の保存 ID を作る。
	/// </summary>
	/// <param name="category">保存カテゴリ。</param>
	/// <param name="slotIndex">カテゴリ内スロット番号。</param>
	/// <returns>拡張子なしの MotionData ID。</returns>
	std::string BuildMotionDataId(CustomizeAttackCategory category, int slotIndex) const;

	/// <summary>
	/// 技カテゴリごとのスロット数を取得する。
	/// </summary>
	/// <param name="category">確認する技カテゴリ。</param>
	/// <returns>対象カテゴリのスロット数。</returns>
	int GetSlotCount(CustomizeAttackCategory category) const;

	/// <summary>
	/// 汎用モーション編集時のプレビュー用 AttackData を用意する。
	/// </summary>
	/// <param name="motionDataId">表示する汎用 MotionData ID。</param>
	void PrepareCommonMotionPreview(const std::string& motionDataId);

	/// <summary>
	/// AttackData 下書きを取得する。
	/// </summary>
	/// <returns>編集可能な AttackData 参照。</returns>
	AttackData& GetDraft();

	/// <summary>
	/// AttackData 下書きを取得する。
	/// </summary>
	/// <returns>読み取り専用の AttackData 参照。</returns>
	const AttackData& GetDraft() const;

	/// <summary>
	/// 編集中 AttackData ID を取得する。
	/// </summary>
	/// <returns>編集中 AttackData ID。</returns>
	const std::string& GetEditingAttackDataId() const;

	/// <summary>
	/// 編集中カテゴリを取得する。
	/// </summary>
	/// <returns>編集中カテゴリ。</returns>
	CustomizeAttackCategory GetSelectedCategory() const;

	/// <summary>
	/// 編集中スロット番号を取得する。
	/// </summary>
	/// <returns>編集中スロット番号。</returns>
	int GetSelectedSlotIndex() const;

	/// <summary>
	/// 表示名入力用バッファを取得する。
	/// </summary>
	/// <returns>ImGui InputText に渡す固定長バッファ。</returns>
	std::array<char, AttackNameBufferSize>& GetDisplayNameBuffer();

	/// <summary>
	/// MotionData ID 入力用バッファを取得する。
	/// </summary>
	/// <returns>ImGui 表示に使う固定長バッファ。</returns>
	std::array<char, MotionDataIdBufferSize>& GetMotionDataIdBuffer();

	/// <summary>
	/// MotionData ID 入力用バッファを取得する。
	/// </summary>
	/// <returns>読み取り専用の固定長バッファ。</returns>
	const std::array<char, MotionDataIdBufferSize>& GetMotionDataIdBuffer() const;

private:
	/// <summary>
	/// 未保存スロットを開いた時に使う初期 AttackData を作る。
	/// </summary>
	/// <param name="category">作成する技カテゴリ。</param>
	/// <param name="slotIndex">カテゴリ内スロット番号。</param>
	/// <param name="attackDataId">保存先 AttackData ID。</param>
	/// <returns>編集開始用の初期 AttackData。</returns>
	AttackData CreateDefaultAttackData(
		CustomizeAttackCategory category,
		int slotIndex,
		const std::string& attackDataId) const;

	/// <summary>
	/// AttackBox の位置と大きさを編集する UI を描画する。
	/// </summary>
	void DrawHitboxEditor();

	/// <summary>
	/// キャンセル可能フレームとキャンセル種別の UI を描画する。
	/// </summary>
	/// <param name="previewTotalFrames">プレビュー表示上の総フレーム数。</param>
	void DrawCancelSettingEditor(int previewTotalFrames);

	/// <summary>
	/// draft の表示名を ImGui 入力用固定バッファへコピーする。
	/// </summary>
	void CopyDisplayNameToBuffer();

	/// <summary>
	/// draft の MotionData ID を ImGui 入力用固定バッファへコピーする。
	/// </summary>
	void CopyMotionDataIdToBuffer();

	AttackData draft;
	std::array<char, AttackNameBufferSize> displayNameBuffer = {};
	std::array<char, MotionDataIdBufferSize> motionDataIdBuffer = {};
	std::string editingAttackDataId;
	CustomizeAttackCategory selectedCategory = CustomizeAttackCategory::Ground;
	int selectedSlotIndex = 0;
	std::array<std::array<CustomizeAttackSlotSummary, MaxAttackSlotCount>, 3> slotSummaries = {};
};
