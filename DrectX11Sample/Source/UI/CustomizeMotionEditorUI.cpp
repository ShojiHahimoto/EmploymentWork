#include "UI/CustomizeEditorUI.h"
#pragma execution_character_set("utf-8")
#include "Controller/CustomizeMotionEditorController.h"
#include "Data/PosePreset.h"
#include "Input/UIInputSystem.h"
#include "Manager/LoadWorker.h"
#include "Manager/SaveWorker.h"
#include "UI/UITheme.h"
#include <algorithm>
#include <cstdio>
#include <chrono>

/// <summary>画面内のプリセット読み込みに使う共通ワーカーを借りる。</summary>
/// <param name="worker">Gameが所有するロードワーカー。</param>
/// <param name="writer">Gameが所有する保存ワーカー。</param>
void CustomizeEditorUI::SetWorkers(LoadWorker *worker, SaveWorker *writer)
{
	loadWorker = worker;
	saveWorker = writer;
}

/// <summary>ポーズプリセットの保存結果を受け取っていないか確認する。</summary>
/// <returns>未受け取りならtrue。</returns>
bool CustomizeEditorUI::IsPresetSavePending() const
{
	return pendingPresetSave.valid();
}

/// <summary>プレビュー内の専用UIを除外する矩形を取得する。</summary>
/// <param name="width">画面幅。</param>
/// <param name="height">画面高さ。</param>
/// <returns>プレビュー上部ツールバー。</returns>
RECT CustomizeEditorUI::PreviewToolbar(int width, int height) const
{
	if (!motionScreenActive)
	{
		return {};
	}

	const RECT region = PreviewRegion(width, height);

	return {region.left + 4, region.top + 4, std::min(region.right, region.left + 392), region.top + 38};
}

/// <summary>モーション画面固有の操作とプリセットダイアログを登録する。</summary>
/// <param name="motion">編集対象。</param>
/// <param name="preview">プレビュー。</param>
/// <param name="common">汎用モーションか。</param>
/// <param name="top">右パネル上端。</param>
/// <param name="bottom">右パネル下端。</param>
/// <param name="split">左右境界。</param>
/// <param name="width">画面幅。</param>
void CustomizeEditorUI::BuildMotionControls(CustomizeMotionEditorController &motion,
											CustomizePreviewController &preview, bool common, int top, int bottom,
											int split, int width)
{
	constexpr int PastePanelHeight = 44;
	constexpr int MirrorPanelHeight = 152;
	const int inlineHeight = (presetPanelMode == 0 ? 0 : 116) + (pastePanelOpen ? PastePanelHeight : 0)
		+ (mirrorPanelOpen ? MirrorPanelHeight : 0);

	root.BeginScrollArea("motion/controls", {split + 8, top + 8, width - 8, bottom - 96},
						 (common ? 590 : 550) + inlineHeight, motionPanelScroll);
	const int left = 16, right = width - split - 32, gap = 16;
	const int half = (left + right) / 2;
	const int frame = preview.GetActionFrame();
	const bool keyed = motion.HasMotionKeyframe(frame);

	const auto heading = [&](const char *id, const char *label, int y)
	{
		root.AddImage(std::string(id) + "/accent", {left, y + 4, left + 7, y + 26}, GameUI::Theme::SectionAccentImage);
		root.AddLabel(id, {left + 18, y, right, y + 28}, label, true, 20, false, true);
	};

	// 既存の汎用モーション総尺・ループ設定も専用UIから編集可能に残す。
	int y = 8;
	heading("pose/heading", "ポーズキーフレーム", y);
	y += 42;
	root.AddIconButton("pose/add", {left, y, half - gap / 2, y + 44}, "ポーズキーを追加", "add", frame >= 0);
	root.AddIconButton("pose/delete", {half + gap / 2, y, right, y + 44}, "ポーズキーを削除", "delete", keyed);
	y += 64;

	heading("part/heading", "選択部位", y);
	y += 42;

	const int third = (right - left - gap * 2) / 3;
	root.AddIconButton("pose/copy", {left, y, left + third, y + 42}, "コピー", "copy", keyed);
	root.AddIconButton("pose/paste", {left + third + gap, y, left + third * 2 + gap, y + 42}, "ペースト", "paste",
					   keyed && motion.HasCopiedPose());
	root.AddIconButton("pose/mirror", {left + (third + gap) * 2, y, right, y + 42},
		"ミラーリング", "mirror", keyed, true, false, 16.0f);
	const int pasteTop = y;
	y += 62;

	if (pastePanelOpen)
	{
		const auto selected = motion.GetSelectedBodyParts();
		const bool hasSelection = std::find(selected.begin(), selected.end(), true) != selected.end();
		const bool canPaste = keyed && motion.HasCopiedPose();
		const int applyY = pasteTop + 48;
		constexpr int applyGap = 8;
		const int applyWidth = std::max(0, (right - left - 2 * applyGap) / 3);
		const int selectedLeft = left + applyWidth + applyGap;
		const int cancelLeft = selectedLeft + applyWidth + applyGap;

		constexpr float applyFontSize = 16.0f;
		root.AddButton("pose/paste/all", {left, applyY, left + applyWidth, applyY + 34},
			"全身に適用", canPaste, applyFontSize);
		root.AddButton("pose/paste/selected", {selectedLeft, applyY, selectedLeft + applyWidth, applyY + 34},
			"選択部位のみ適用", canPaste && hasSelection, applyFontSize);
		root.AddButton("pose/paste/cancel", {cancelLeft, applyY, cancelLeft + applyWidth, applyY + 34},
			"キャンセル", true, applyFontSize);

		y += PastePanelHeight;
		pastePanelBounds = {split + 8 + left, top + 8 + pasteTop - motionPanelScroll,
			split + 8 + right, top + 8 + y - motionPanelScroll};
	}

	if (mirrorPanelOpen)
	{
		const int panelTop = pasteTop + 48;
		root.AddLabel("pose/mirror/directionLabel", {left, panelTop, half, panelTop + 30}, "適用方向", true, 16.0f);
		root.AddLabel("pose/mirror/right", {half, panelTop, half + 30, panelTop + 30}, "右", true, 16.0f, true);
		root.AddButton("pose/mirror/direction", {half + 30, panelTop, half + 78, panelTop + 30},
			mirrorRightToLeft ? "→" : "←", true, 16.0f);
		root.AddLabel("pose/mirror/left", {half + 78, panelTop, half + 108, panelTop + 30}, "左", true, 16.0f, true);

		const char* labels[] = {"肩", "肘", "手首", "股関節", "膝", "足首"};
		const int groupRight = left + (right - left) * 28 / 100;
		const int jointWidth = (right - groupRight) / 3;
		for (int group = 0; group < 2; ++group)
		{
			const int first = group * 3;
			const int row = panelTop + 34 + group * 34;
			mirrorGroups[group] = mirrorParts[first] && mirrorParts[first + 1] && mirrorParts[first + 2];
			const std::string groupId = "pose/mirror/group/" + std::to_string(group);
			root.AddCheckbox(groupId, {left, row, groupRight, row + 30},
				group == 0 ? "腕全体" : "脚全体", mirrorGroups[group], true, 16.0f);

			for (int joint = 0; joint < 3; ++joint)
			{
				const int index = first + joint;
				const int x = groupRight + joint * jointWidth;
				root.AddCheckbox("pose/mirror/joint/" + std::to_string(index), {x, row, x + jointWidth, row + 30},
					labels[index], mirrorParts[index], true, 16.0f);
			}
		}

		const int applyY = panelTop + 106;
		const bool hasParts = std::find(mirrorParts.begin(), mirrorParts.end(), true) != mirrorParts.end();
		root.AddButton("pose/mirror/apply", {left, applyY, half - 4, applyY + 34}, "適用", keyed && hasParts, 16.0f);
		root.AddButton("pose/mirror/cancel", {half + 4, applyY, right, applyY + 34}, "キャンセル", true, 16.0f);

		y += MirrorPanelHeight;
		mirrorPanelBounds = {split + 8 + left, top + 8 + pasteTop - motionPanelScroll,
			split + 8 + right, top + 8 + y - motionPanelScroll};
	}

	const int presetTop = y;
	heading("preset/heading", "ポーズプリセット", y);
	y += 42;
	root.AddIconButton("preset/save", {left, y, half - gap / 2, y + 42}, "保存", "preset_save", keyed);
	root.AddIconButton("preset/load", {half + gap / 2, y, right, y + 42}, "読み込み", "preset_load", keyed);
	y += 52;
	BuildInlinePosePreset(motion, keyed, left, right, y);
	y += 10;
	presetPanelBounds = {split + 8 + left, top + 8 + presetTop - motionPanelScroll, split + 8 + right,
						 top + 8 + y - motionPanelScroll};

	heading("motionPreset/heading", "モーションプリセット", y);
	y += 42;
	root.AddIconButton("motionPreset/save", {left, y, half - gap / 2, y + 42}, "保存", "preset_save");
	root.AddIconButton("motionPreset/load", {half + gap / 2, y, right, y + 42}, "読み込み", "preset_load");
	y += 62;

	heading("movement/heading", "移動キーフレーム", y);
	y += 42;
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
	root.AddButton("motion", {split + 48, bottom - 58, width - 48, bottom - 18},
				   common ? "モーション選択へ戻る" : "パラメータ編集");
}

/// <summary>専用UIのモーション操作結果だけをControllerへ渡す。</summary>
/// <param name="motion">操作先。</param>
/// <param name="preview">プレビュー。</param>
/// <param name="common">汎用モーションか。</param>
/// <param name="status">操作結果。</param>
/// <param name="previewRegion">部位選択を継続できるプレビュー範囲。</param>
void CustomizeEditorUI::ApplyMotionControls(CustomizeMotionEditorController &motion,
											CustomizePreviewController &preview, bool common, std::string &status,
											const GameUI::Rect &previewRegion)
{
	message = UserNotification(status);

	const int frame = preview.GetActionFrame();
	const int total = common ? motion.GetDraft().totalFrames : GetAttackTotalFrames(edit.attackData.frame);

	if (motion.IsMultipleSelectionEnabled() != multipleSelectionEnabled)
	{
		preview.CancelGizmoEditing();
		motion.SetMultipleSelectionEnabled(multipleSelectionEnabled);
	}

	if (root.WasActivated("select/all"))
	{
		preview.CancelGizmoEditing();
		motion.SelectAllBodyParts();
		motion.RefreshFrameEditValues(frame);
	}

	if (root.WasActivated("select/none"))
	{
		preview.CancelGizmoEditing();
		motion.SelectBodyPart(preview, -1, frame);
	}

	std::snprintf(motion.GetDisplayNameBuffer().data(), motion.GetDisplayNameBuffer().size(), "%s", motionName.c_str());

	if (root.WasActivated("pose/add"))
	{
		status = motion.AddWholeBodyKeyframe(frame, total);
		movementSelected = false;
	}

	if (root.WasActivated("pose/delete"))
	{
		status = motion.DeleteWholeBodyKeyframe(frame);
	}

	if (root.WasActivated("pose/copy"))
	{
		status = motion.CopyWholeBodyPose(frame);
	}

	if (root.WasActivated("pose/paste"))
	{
		pastePanelOpen = !pastePanelOpen;
		mirrorPanelOpen = false;
		presetPanelMode = 0;
	}

	const bool pasteAll = root.WasActivated("pose/paste/all");
	const bool pasteSelected = root.WasActivated("pose/paste/selected");
	if (pastePanelOpen && (pasteAll || pasteSelected))
	{
		auto mask = motion.GetSelectedBodyParts();
		if (pasteAll)
		{
			mask.fill(true);
		}

		if (std::find(mask.begin(), mask.end(), true) != mask.end())
		{
			preview.Stop();
			motion.EndIkEdit(preview);
			status = motion.PasteCopiedPose(frame, mask);
		}

		pastePanelOpen = false;
	}

	if (pastePanelOpen && (root.WasActivated("pose/paste/cancel")
		|| !motion.HasMotionKeyframe(frame)
		|| root.ShouldCollapseInlineSection("pose/paste", pastePanelBounds, previewRegion)))
	{
		pastePanelOpen = false;
	}

	if (root.WasActivated("pose/mirror"))
	{
		mirrorPanelOpen = !mirrorPanelOpen;
		pastePanelOpen = false;
		presetPanelMode = 0;
	}

	if (mirrorPanelOpen)
	{
		if (root.WasActivated("pose/mirror/direction"))
		{
			mirrorRightToLeft = !mirrorRightToLeft;
		}

		for (int group = 0; group < 2; ++group)
		{
			if (root.WasActivated("pose/mirror/group/" + std::to_string(group)))
			{
				std::fill_n(mirrorParts.begin() + group * 3, 3, mirrorGroups[group]);
			}
		}

		if (root.WasActivated("pose/mirror/apply"))
		{
			preview.Stop();
			motion.EndIkEdit(preview);
			status = motion.MirrorPose(preview, frame, mirrorRightToLeft, mirrorParts);
			mirrorPanelOpen = false;
		}

		if (root.WasActivated("pose/mirror/cancel") || !motion.HasMotionKeyframe(frame)
			|| root.ShouldCollapseInlineSection("pose/mirror", mirrorPanelBounds, previewRegion))
		{
			mirrorPanelOpen = false;
		}
	}

	if (root.WasActivated("gizmo/rotate"))
	{
		preview.translationGizmoEnabled = false;
		movementSelected = false;
	}

	if (root.WasActivated("gizmo/move"))
	{
		preview.translationGizmoEnabled = true;
		movementSelected = false;
	}

	if (root.WasActivated("movement/add"))
	{
		if (common)
		{
			status = motion.AddRootOffsetKeyframe(frame);
		}
		else
		{
			DirectX::SimpleMath::Vector2 value;
			motion.AddAttackMovementKeyframe(edit.attackData, frame, value);
		}

		movementSelected = frame >= 0;
	}

	if (root.WasActivated("movement/delete"))
	{
		if (common)
		{
			status = motion.DeleteRootOffsetKeyframe(frame);
		}
		else
		{
			DirectX::SimpleMath::Vector2 value;
			motion.DeleteAttackMovementKeyframe(edit.attackData, frame, value);
		}
	}

	if (root.WasActivated("preset/save"))
	{
		preview.Stop();
		pastePanelOpen = false;
		presetPanelMode = presetPanelMode == 1 ? 0 : 1;
		mirrorPanelOpen = false;
	}

	if (root.WasActivated("preset/load") && !pendingPresetSave.valid())
	{
		preview.Stop();
		pastePanelOpen = false;
		presetPanelMode = presetPanelMode == 2 ? 0 : 2;
		mirrorPanelOpen = false;
		presetIndex = 0;
		presetIds.clear();
		presetNames.clear();
		loadedPresets.clear();
		pendingPresets = {};

		if (presetPanelMode == 2 && loadWorker)
		{
			auto task = std::make_shared<std::packaged_task<std::vector<PosePresetData>()>>(
				&CustomizeMotionEditorController::PreparePosePresets);
			pendingPresets = task->get_future();
			loadWorker->Submit(std::packaged_task<void()>([task]
			{
				(*task)();
			}));
		}
	}

	// 仮ボタンは通常の濃さで描き、未実装の機能を実行したようには見せない。
	for (const char *id : {"motionPreset/save", "motionPreset/load"})
	{
		if (root.WasActivated(id))
		{
			status = "この機能は今後実装します。";
		}
	}

	if (root.WasActivated("preset/confirmSave") && saveWorker && !pendingPresetSave.valid() &&
		!pendingPresets.valid())
	{
		status = "保存中";
		pendingPresetSave = saveWorker->Enqueue([draft = motion, frame, name = presetName]() mutable
		{
			return draft.SavePosePreset(frame, name);
		});
	}

	if ((root.WasActivated("preset/applyAll") || root.WasActivated("preset/applySelected")) && presetIndex >= 0 &&
		presetIndex < static_cast<int>(presetIds.size()))
	{
		auto mask = motion.GetSelectedBodyParts();

		if (root.WasActivated("preset/applyAll"))
		{
			mask.fill(true);
		}

		status = motion.ApplyPosePreset(loadedPresets[presetIndex], frame, mask);
	}

	if (root.WasActivated("preset/cancel"))
	{
		presetPanelMode = 0;
	}

	if (presetPanelMode != 0)
	{
		// プレビューの部位選択は適用先の指定なので継続する。他項目へのフォーカスで閉じる。
		if (root.ShouldCollapseInlineSection("preset/", presetPanelBounds, previewRegion))
		{
			presetPanelMode = 0;
		}
	}

	if (presetPanelMode != 2)
	{
		pendingPresets = {};
	}
	else if (pendingPresets.valid() &&
			 pendingPresets.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
	{
		try
		{
			loadedPresets = pendingPresets.get();
			for (const auto &preset : loadedPresets)
			{
				presetIds.push_back(preset.presetId);
				presetNames.push_back(preset.displayName);
			}
		}
		catch (const std::exception &)
		{
			status = "ポーズプリセットを読み込めませんでした。";
		}
	}
}

/// <summary>プリセットボタン直下へ保存/読み込みの入力欄を登録する。</summary>
/// <param name="motion">適用対象の選択状態。</param>
/// <param name="keyed">現在フレームに姿勢キーがあるか。</param>
/// <param name="left">内容領域左端。</param>
/// <param name="right">内容領域右端。</param>
/// <param name="y">追加先の行位置。登録した高さだけ進める。</param>
void CustomizeEditorUI::BuildInlinePosePreset(CustomizeMotionEditorController &motion, bool keyed, int left, int right,
											  int &y)
{
	if (presetPanelMode == 0)
	{
		return;
	}

	root.AddLabel("preset/inputLabel", {left, y, left + 116, y + 34},
				  presetPanelMode == 1 ? "ポーズ名" : "ポーズを選択", true, 16);
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
		const bool anySelected = std::any_of(selected.begin(), selected.end(),
											 [](bool value)
											 {
												 return value;
											 });
		root.AddIconButton("preset/applyAll", {left, y, left + column, y + 34}, "全身に適用", "",
						   keyed && !presetIds.empty(), false, false, 16);
		root.AddIconButton("preset/applySelected", {left + column + 8, y, left + column * 2 + 8, y + 34},
						   "選択部位のみ適用", "", keyed && anySelected && !presetIds.empty(), false, false, 16);
		root.AddIconButton("preset/cancel", {left + (column + 8) * 2, y, right, y + 34}, "キャンセル", "", true, false,
						   false, 16);
	}

	y += 42;
	root.AddLabel("preset/result", {left, y, right, y + 24}, message, true, 14);
	y += 30;
}
