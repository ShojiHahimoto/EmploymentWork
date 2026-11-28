#include "Controller/CustomizeAttackFlowController.h"

#pragma execution_character_set("utf-8")
#include "Input/InputSystem.h"
#include "Input/InputTypes.h"
#include "Manager/SceneManager.h"
#include "Manager/LoadWorker.h"
#include "Manager/SaveWorker.h"
#include "Scene/CustomizeSelectScene.h"

#include <algorithm>
#include <chrono>
#include <utility>

using namespace DirectX::SimpleMath;

namespace
{
/// <summary>UI操作の区切りだけに使う単調増加時刻を返す。</summary>
/// <returns>秒単位の時刻。ゲームのΔTimeや進行フレームには使用しない。</returns>
double EditHistoryTime()
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
} // namespace

/// <summary>編集対象を保持する。下書きとプレビューの準備は Enter まで行わない。</summary>
/// <param name="initialWidth">初期画面幅。</param>
/// <param name="initialHeight">初期画面高さ。</param>
/// <param name="target">技または汎用モーションの編集対象。</param>
CustomizeAttackFlowController::CustomizeAttackFlowController(int initialWidth, int initialHeight,
	CustomizeAttackTarget target)
	: target(std::move(target)), width(initialWidth), height(initialHeight)
{
}

/// <summary>準備済みの下書きを使い、プレビューを初期化する。</summary>
/// <param name="worker">画面内の追加読み込みに使う共通ワーカー。</param>
/// <param name="writer">下書きの保存に使う専用ワーカー。</param>
void CustomizeAttackFlowController::Enter(LoadWorker *worker, SaveWorker *writer)
{
	Input::InputSystem::SetActionMap(Input::InputActionMapId::UI);
	editorUI.SetWorkers(worker, writer);
	saveWorker = writer;
	previewController.Initialize();
}

/// <summary>技とモーションの下書きをGPUや共有キャッシュに触れずに準備する。</summary>
/// <param name="target">選択した技または汎用モーション。</param>
/// <returns>初期化済みの編集データ。</returns>
PreparedAttackDraft CustomizeAttackFlowController::PrepareDraft(const CustomizeAttackTarget &target)
{
	PreparedAttackDraft prepared;

	if (const auto *slot = std::get_if<CustomizeAttackSlot>(&target))
	{
		prepared.attack.SelectSlot(slot->category, slot->index);
		const std::string oldId = prepared.attack.GetDraft().motionDataId;
		prepared.attack.EnsureDraftMotionDataId();

		const auto &attack = prepared.attack.GetDraft();
		const std::string legacy = oldId.starts_with("Attack/") ? oldId : std::string{};
		prepared.status = prepared.motion.LoadDraft(attack.motionDataId, attack.motionDataId,
													GetAttackTotalFrames(attack.frame), false, legacy);
	}
	else
	{
		const auto &common = std::get<CustomizeCommonMotionSlot>(target);
		prepared.commonMotionId = "Common/" + common.name;
		prepared.attack.PrepareCommonMotionPreview(prepared.commonMotionId);
		prepared.status = prepared.motion.LoadDraft(prepared.commonMotionId, common.name, 30, true);
	}

	return prepared;
}

/// <summary>ワーカーで準備した下書きを遷移先へ移す。</summary>
/// <param name="prepared">編集開始時に使用する下書き。</param>
void CustomizeAttackFlowController::ApplyPreparedDraft(PreparedAttackDraft &&prepared)
{
	attackEditor = std::move(prepared.attack);
	motionEditor = std::move(prepared.motion);
	statusMessage = std::move(prepared.status);
	editingCommonMotionId = std::move(prepared.commonMotionId);

	editingCommonMotion = !editingCommonMotionId.empty();
	mode = editingCommonMotion ? CustomizeMode::MotionEditor : CustomizeMode::AttackEditor;
}

/// <summary>シーン遷移前に読み込む編集対象を取得する。</summary>
/// <returns>このコントローラーが保持する技スロットまたは汎用モーション。</returns>
const CustomizeAttackTarget &CustomizeAttackFlowController::GetTarget() const
{
	return target;
}

/// <summary>編集シーン固有のプレビューを解放する。モデルは共有キャッシュに残す。</summary>
void CustomizeAttackFlowController::Exit()
{
	previewController.Release();
}

void CustomizeAttackFlowController::Update()
{
	ApplySaveResult();
	UpdatePreviewPlayback();
	UpdateAttackEditorControls();
}

/// <summary>編集画面の背景を描画する。</summary>
/// <param name="renderer">描画先のレンダラー。</param>
void CustomizeAttackFlowController::DrawBackdrop(Renderer &renderer)
{
	editorUI.DrawBackdrop(renderer, width, height);
}

/// <summary>編集対象の3Dプレビューを描画する。</summary>
/// <param name="renderer">描画先のレンダラー。</param>
void CustomizeAttackFlowController::DrawPreview(Renderer &renderer)
{
	EnsureDraftAttackMotionDataId();

	ClampPreviewCurrentFrame();

	const RECT region = CustomizeEditorUI::PreviewRegion(width, height);
	RenderAttackPreview(renderer, &region);
}

/// <summary>プレビューの上に編集UIを描画する。</summary>
/// <param name="renderer">描画先のレンダラー。</param>
void CustomizeAttackFlowController::DrawInterface(Renderer &renderer)
{
	editorUI.Draw(renderer);
}

/// <summary>
/// ウィンドウサイズ変更後の ImGui 配置用サイズを更新する。
/// </summary>
/// <param name="newWidth">新しい幅。</param>
/// <param name="newHeight">新しい高さ。</param>
void CustomizeAttackFlowController::OnResize(int newWidth, int newHeight)
{
	if (newWidth <= 0 || newHeight <= 0)
	{
		return;
	}

	width = newWidth;
	height = newHeight;
}

/// <summary>編集対象に対応するスロット一覧への遷移を予約する。</summary>
void CustomizeAttackFlowController::NavigateBack()
{
	if (pendingSave.valid() || editorUI.IsPresetSavePending())
	{
		return;
	}

	if (const auto *slot = std::get_if<CustomizeAttackSlot>(&target))
	{
		SceneManager::GetInstance().RequestChangeScene(
			std::make_unique<CustomizeSelectScene>(width, height, CustomizeMode::AttackSlotSelect, slot->category));

		return;
	}

	SceneManager::GetInstance().RequestChangeScene(
		std::make_unique<CustomizeSelectScene>(width, height, CustomizeMode::CommonMotionSelect));
}

/// <summary>共通編集UIの操作結果を受け、保存と画面遷移を振り分ける。</summary>
void CustomizeAttackFlowController::UpdateAttackEditorControls()
{
	// 操作前の状態を保存
	motionEditor.ConfigurePoseSampling(previewController, !editingCommonMotion, attackEditor.GetDraft().usableState);

	const auto before = CaptureEditSnapshot();
	const bool gizmoWasActive = previewController.IsGizmoEditing();

	// 実際のUI操作部分
	editorUI.Update(attackEditor, previewController, width, height, statusMessage, motionEditor,
					mode == CustomizeMode::MotionEditor, editingCommonMotion, editHistory.CanUndo(),
					editHistory.CanRedo());
	const CustomizeAttackEditorAction action = editorUI.GetRequestedAction();

	// 操作前と操作後の差分を比較し、変更があれば残す
	const auto after = CaptureEditSnapshot();
	const auto interaction = editorUI.GetEditInteraction();
	const bool changed = !CustomizeEditHistory::Equal(before, after);

	// 編集中はプレビューを停止する
	if (changed || interaction.ongoing)
	{
		previewController.Stop();
	}

	// 変更があった場合履歴管理に知らせる処理
	//  現状imguizumoを描画側で処理しているため、Gizumo操作中に履歴を区切らないための条件分岐
	if (!gizmoWasActive || changed)
	{
		editHistory.Observe(before, after, interaction.id, interaction.ongoing, interaction.wheel, EditHistoryTime());
	}

	// 以下各操作ごとの処理
	if (action != CustomizeAttackEditorAction::None)
	{
		editHistory.Commit();
		previewController.CancelGizmoEditing();
	}

	if (action == CustomizeAttackEditorAction::Undo || action == CustomizeAttackEditorAction::Redo)
	{
		previewController.Stop();
		previewController.CancelGizmoEditing();

		CustomizeEditSnapshot restored;

		const bool found =
			action == CustomizeAttackEditorAction::Undo ? editHistory.Undo(restored) : editHistory.Redo(restored);

		statusMessage =
			found ? (action == CustomizeAttackEditorAction::Undo ? "操作を元に戻しました。" : "操作をやり直しました。")
				  : "対象の操作はありません。";

		if (found)
		{
			RestoreEditSnapshot(restored);
		}

		return;
	}

	if (action == CustomizeAttackEditorAction::Save)
	{
		SaveDraftMotion();

		return;
	}

	if (action == CustomizeAttackEditorAction::OpenAttackEditor)
	{
		mode = CustomizeMode::AttackEditor;

		return; // 編集画面同士の切替なので、下書きとカメラを維持する。
	}

	if (action == CustomizeAttackEditorAction::OpenMotionEditor)
	{
		mode = CustomizeMode::MotionEditor;
		ClampPreviewCurrentFrame();

		return;
	}

	if (action == CustomizeAttackEditorAction::Back)
	{
		NavigateBack();

		return;
	}

	ClampPreviewCurrentFrame();
}

/// <summary>
/// 編集中の技に、MotionData ID を割り当てる。
/// </summary>
void CustomizeAttackFlowController::EnsureDraftAttackMotionDataId()
{
	if (editingCommonMotion)
	{
		return;
	}

	attackEditor.EnsureDraftMotionDataId();
}

/// <summary>
/// 編集中の MotionData 下書きを JSON として保存する。
/// </summary>
void CustomizeAttackFlowController::SaveDraftMotion()
{
	if (!saveWorker || pendingSave.valid() || !motionEditor.HasDraft())
	{
		return;
	}

	EnsureDraftAttackMotionDataId();

	const std::string motionId = GetEditingMotionDataId();
	const int total = GetPreviewTotalFrames();
	const bool common = editingCommonMotion;
	const bool looping = common && motionEditor.GetDraft().looping;
	statusMessage = "保存中";
	pendingSave = saveWorker->Enqueue([motion = motionEditor, attack = attackEditor, motionId, total, common, looping]() mutable
	{
		const auto motionResult = motion.SaveDraft(motionId, total, looping);
		if (!motionResult.starts_with("Saved MotionData:"))
		{
			return motionResult;
		}

		if (!common && !attack.SaveDraft().starts_with("Saved:"))
		{
			return std::string("MotionData saved, but AttackData save failed. Retry saving both files.");
		}

		return common ? std::string("モーションを保存しました。") : std::string("技とモーションを保存しました。");
	});
}

/// <summary>完了した保存結果を画面に反映する。</summary>
void CustomizeAttackFlowController::ApplySaveResult()
{
	if (!pendingSave.valid() || pendingSave.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
	{
		return;
	}

	try
	{
		statusMessage = pendingSave.get();
	}
	catch (const std::exception &)
	{
		statusMessage = "保存に失敗しました。";
	}
}

/// <summary>
/// 再生中なら表示フレームを 1 つ進め、技プレビュー終端に到達したら停止する。
/// </summary>
void CustomizeAttackFlowController::UpdatePreviewPlayback()
{
	const bool isPreviewMode = mode == CustomizeMode::AttackEditor || mode == CustomizeMode::MotionEditor;
	const bool advanced = previewController.UpdatePlayback(
		isPreviewMode, editingCommonMotion && motionEditor.GetDraft().looping, GetPreviewTotalFrames());
	if (!advanced)
	{
		return;
	}

	const int actionFrame = GetPreviewActionFrame();

	if (actionFrame >= 0)
	{
		const AttackData &draftAttack = attackEditor.GetDraft();
		motionEditor.RefreshFrameEditValues(actionFrame);
		attackMovementKeyOffset = motionEditor.GetAttackMovementOffsetAtFrame(draftAttack, actionFrame);
	}
}

/// <summary>
/// プレビュー用カメラでモデルを描画する。本体矩形が渡された場合はバックバッファへ直接描画する。
/// </summary>
/// <param name="renderer">描画に使う Renderer。</param>
/// <param name="region">本体の描画矩形。nullptr の場合は従来の RenderTexture。</param>
void CustomizeAttackFlowController::RenderAttackPreview(Renderer &renderer, const RECT *region)
{
	const auto beforeEdit = CaptureEditSnapshot();
	const int actionFrame = GetPreviewActionFrame();
	const bool motionMode = mode == CustomizeMode::MotionEditor;
	const bool movementMode = motionMode && editorUI.IsMovementSelected();
	previewController.rootMovementGizmoEnabled =
		movementMode && !previewController.IsPlaying() &&
		(editingCommonMotion ? motionEditor.HasRootOffsetKeyframe(actionFrame)
							 : motionEditor.HasAttackMovementKeyframe(attackEditor.GetDraft(), actionFrame));
	if (motionMode)
	{
		previewController.editorToolbarExclusions = editorUI.PreviewToolbarExclusions();
	}
	else
	{
		previewController.editorToolbarExclusions.clear();
	}

	previewController.editorInputBlocked = false; // プリセットはパネル内展開なのでプレビュー全体を遮断しない。
	previewController.showJointMarkers = editorUI.AreMarkersVisible();
	previewController.selectedMarkerParts = motionEditor.GetSelectedBodyParts();

	const bool rotationGizmoEnabled = mode == CustomizeMode::MotionEditor && region != nullptr &&
									  !previewController.IsPlaying() && !movementMode &&
									  motionEditor.HasMotionKeyframe(actionFrame);

	previewController.Render(renderer, region, attackEditor.GetDraft(), motionEditor.GetDraft(), editingCommonMotion,
							 motionEditor.HasDraft(), GetEditingMotionDataId(), motionEditor.GetSelectedBoneIndex(),
							 rotationGizmoEnabled);
	if (motionMode)
	{
		const int picked = previewController.ConsumePickedBodyPartIndex();

		if (!movementMode && picked != -1)
		{
			motionEditor.SelectBodyPart(previewController, picked == -2 ? -1 : picked, actionFrame);
		}

		motionEditor.ConsumeTranslationEdit(previewController, actionFrame);
	}

	if (previewController.rootMovementChanged)
	{
		previewController.rootMovementChanged = false;

		if (previewController.rootMovementGizmoEnabled)
		{
			const Vector3 offset = previewController.rootMovementOffset;

			if (editingCommonMotion)
			{
				motionEditor.GetRootOffsetKey() = offset;
				statusMessage = motionEditor.SetRootOffsetKeyframe(actionFrame);
			}
			else
			{
				motionEditor.UpdateAttackMovementKeyframe(attackEditor.GetDraft(), actionFrame,
														  Vector2(offset.x, offset.y));
			}
		}
	}

	int gizmoBodyPartIndex = -1;
	int gizmoAxisIndex = -1;

	Quaternion gizmoLocalRotation = Quaternion::Identity;

	if (previewController.ConsumeGizmoRotationEdit(gizmoBodyPartIndex, gizmoAxisIndex, gizmoLocalRotation))
	{
		statusMessage = motionEditor.ApplyGizmoRotationEdit(gizmoBodyPartIndex, gizmoAxisIndex, actionFrame,
															GetPreviewTotalFrames(), gizmoLocalRotation);
	}

	const auto afterEdit = CaptureEditSnapshot();

	if (previewController.IsGizmoEditing() || editHistory.IsPending("gizmo") ||
		!CustomizeEditHistory::Equal(beforeEdit, afterEdit))
	{
		editHistory.Observe(beforeEdit, afterEdit, "gizmo", previewController.IsGizmoEditing(), false,
							EditHistoryTime());
	}
}

/// <summary>保存対象だけを複製し、現在フレームを復元用に付ける。</summary>
/// <returns>カメラやコピー用姿勢を含まない編集状態。</returns>
CustomizeEditSnapshot CustomizeAttackFlowController::CaptureEditSnapshot()
{
	CustomizeEditSnapshot snapshot;
	snapshot.attack = attackEditor.GetEditState();
	snapshot.motion = motionEditor.GetDraft();
	snapshot.motion.displayName = motionEditor.GetDisplayNameBuffer().data();
	snapshot.commonMotion = editingCommonMotion;
	snapshot.previewFrame = previewController.GetCurrentFrame();

	return snapshot;
}

/// <summary>履歴の下書きを復元し、古い入力・Gizmoの捕捉を終了する。</summary>
/// <param name="snapshot">復元する状態。</param>
void CustomizeAttackFlowController::RestoreEditSnapshot(const CustomizeEditSnapshot &snapshot)
{
	previewController.Stop();
	previewController.CancelGizmoEditing();
	motionEditor.EndIkEdit(previewController);

	if (!editingCommonMotion)
	{
		attackEditor.ApplyEditState(snapshot.attack);
	}

	previewController.SetCurrentFrame(snapshot.previewFrame);
	motionEditor.RestoreDraft(snapshot.motion, snapshot.previewFrame - 1);
	ClampPreviewCurrentFrame();
	motionEditor.RefreshFrameEditValues(GetPreviewActionFrame());
	attackMovementKeyOffset =
		motionEditor.GetAttackMovementOffsetAtFrame(attackEditor.GetDraft(), GetPreviewActionFrame());
	editorUI.ResetAfterHistory();
	// 描画用の値参照も復元直後に作り直す。空入力を使い、Undoボタンを二度実行しない。

	editorUI.Update(attackEditor, previewController, width, height, statusMessage, motionEditor,
					mode == CustomizeMode::MotionEditor, editingCommonMotion, editHistory.CanUndo(),
					editHistory.CanRedo());
}

/// <summary>
/// 現在フレームを、0F の Idle を含むプレビュー表示範囲内へ収める。
/// </summary>
void CustomizeAttackFlowController::ClampPreviewCurrentFrame()
{
	previewController.ClampCurrentFrame(GetPreviewTotalFrames());
}

/// <summary>
/// AttackFrameData の新しい発生フレーム定義から、内部処理上の攻撃総フレーム数を取得する。
/// </summary>
/// <returns>最低 1F を保証した総フレーム数。プレビュー表示では 0F Idle を含めて 0..この値まで表示する。</returns>
int CustomizeAttackFlowController::GetPreviewTotalFrames() const
{
	if (editingCommonMotion)
	{
		return std::max(1, motionEditor.GetDraft().totalFrames);
	}

	const AttackData &draftAttack = attackEditor.GetDraft();

	return GetAttackTotalFrames(draftAttack.frame);
}

/// <summary>
/// プレビュー表示フレームを、内部の PlayerActionState::actionFrame 相当へ変換する。
/// </summary>
/// <returns>0F Idle は -1、1F 以降は 0 始まりの内部 actionFrame。</returns>
int CustomizeAttackFlowController::GetPreviewActionFrame() const
{
	return previewController.GetActionFrame();
}

/// <summary>
/// 現在の編集対象から、読み書きする MotionData ID を取得する。
/// </summary>
/// <returns>Common 編集中は Common ID、Attack 編集中は AttackData の motionDataId。</returns>
std::string CustomizeAttackFlowController::GetEditingMotionDataId() const
{
	if (editingCommonMotion)
	{
		return editingCommonMotionId.empty() ? std::string(attackEditor.GetMotionDataIdBuffer().data())
											 : editingCommonMotionId;
	}

	return attackEditor.GetDraft().motionDataId;
}
