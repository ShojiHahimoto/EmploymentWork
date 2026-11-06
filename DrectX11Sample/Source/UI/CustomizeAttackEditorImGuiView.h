#pragma once

#include "Controller/CustomizeAttackEditorController.h"

#include <string>

/// <summary>
/// 移行期間中だけ使う ImGui 表示アダプター。技編集の保存・検証は Controller に委ねる。
/// </summary>
class CustomizeAttackEditorImGuiView
{
public:
	/// <summary>ImGui 操作を編集スナップショットへ反映し、Controller の操作要求を返す。</summary>
	/// <param name="controller">技編集状態と操作APIを所有するController。</param>
	/// <param name="previewTotalFrames">プレビュー上の総フレーム数。</param>
	/// <param name="statusMessage">保存結果などの表示文字列。</param>
	/// <returns>画面遷移要求。編集を継続する場合は None。</returns>
	CustomizeAttackEditorAction Draw(
		CustomizeAttackEditorController& controller,
		int previewTotalFrames,
		std::string& statusMessage);

private:
	/// <summary>攻撃判定の位置・大きさ・追加削除を描画する。</summary>
	/// <param name="attackData">編集中スナップショットの技データ。</param>
	void DrawHitboxEditor(AttackData& attackData);
	/// <summary>キャンセル範囲と許可種別を描画する。</summary>
	/// <param name="attackData">編集中スナップショットの技データ。</param>
	/// <param name="previewTotalFrames">プレビュー上の総フレーム数。</param>
	void DrawCancelSettingEditor(AttackData& attackData, int previewTotalFrames);
};
