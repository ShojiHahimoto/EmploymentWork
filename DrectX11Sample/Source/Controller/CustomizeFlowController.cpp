#include "Controller/CustomizeFlowController.h"

#pragma execution_character_set("utf-8")
#include "Data/AttackDataSaver.h"
#include "Input/InputSystem.h"
#include "Input/InputTypes.h"
#include "Scene/SceneManager.h"
#include "Scene/TitleScene.h"
#include "System/imgui-docking/imgui.h"

#include <algorithm>
#include <chrono>
#include <sstream>

using namespace DirectX::SimpleMath;

namespace
{
	/// <summary>UI操作の区切りだけに使う単調増加時刻を返す。</summary>
	/// <returns>秒単位の時刻。ゲームのΔTimeや進行フレームには使用しない。</returns>
	double EditHistoryTime()
	{
		return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}
	constexpr const char* CommonIdleMotionDataId = "Common/Idle";
	constexpr const char* CategoryLabels[] = { "Ground", "Air", "Special" };
	constexpr const char* CommonMotionLabels[] = {
		"Idle",
		"WalkForward",
		"WalkBack",
		"Crouch",
		"Guard",
		"CrouchGuard",
		"JumpStart",
		"JumpLoop",
		"Hitstun",
		"AirHitstun",
		"AirToDown",
		"Down",
		"Wakeup"
	};

	/// <summary>
	/// CustomizeAttackCategory を配列アクセス用の番号へ変換する。
	/// </summary>
	/// <param name="category">変換するカテゴリ。</param>
	/// <returns>Ground=0, Air=1, Special=2。</returns>
	int ToCategoryIndex(CustomizeAttackCategory category)
	{
		return static_cast<int>(category);
	}
}

/// <summary>
/// CustomizeFlowController を現在の描画サイズで初期化する。
/// </summary>
/// <param name="initialWidth">初期ウィンドウ幅。</param>
/// <param name="initialHeight">初期ウィンドウ高さ。</param>
CustomizeFlowController::CustomizeFlowController(World& worldReference, int initialWidth, int initialHeight)
	: world(worldReference)
	, width(initialWidth)
	, height(initialHeight)
{
}

/// <summary>
/// カスタマイズ用入力マップに切り替え、初期メニューへ戻す。
/// </summary>
void CustomizeFlowController::Enter()
{
	Input::InputSystem::SetActionMap(Input::InputActionMapId::UI);
	mode = CustomizeMode::MainMenu;
	statusMessage.clear();
	InitializePreview();
}

/// <summary>
/// カスタマイズシーンが保持する一時 World を破棄する。
/// </summary>
void CustomizeFlowController::Exit()
{
	editHistory.Reset();
	ReleasePreview();
}

/// <summary>
/// キャンセル入力があれば、現在の編集階層から一つ戻る。
/// </summary>
void CustomizeFlowController::RunSystems()
{
	UpdatePreviewPlayback();
	if (mode == CustomizeMode::AttackEditor || mode == CustomizeMode::MotionEditor)
	{
		UpdateAttackEditorControls();
		return; // 専用UIが文字編集中の取消と画面の戻る操作を区別する。
	}

	if (WasCancelTriggered())
	{
		NavigateBack();
	}
}

/// <summary>
/// カスタマイズ用の ImGui 画面を描画する。
/// </summary>
/// <param name="renderer">現段階では未使用。後でプレビュー描画に使う。</param>
void CustomizeFlowController::Draw(Renderer& renderer)
{
	switch (mode)
	{
	case CustomizeMode::MainMenu:
		DrawMainMenu();
		break;
	case CustomizeMode::AttackCategorySelect:
		DrawAttackCategorySelect();
		break;
	case CustomizeMode::AttackSlotSelect:
		DrawAttackSlotSelect();
		break;
	case CustomizeMode::AttackEditor:
		DrawAttackEditor(renderer);
		break;
	case CustomizeMode::MotionEditor:
		DrawMotionEditorScreen(renderer);
		break;
	case CustomizeMode::CommonMotionSelect:
		DrawCommonMotionSelect();
		break;
	case CustomizeMode::CharacterSlotSelect:
		mode = characterEditor.DrawSlotSelect(statusMessage);
		break;
	case CustomizeMode::CharacterEditor:
		mode = characterEditor.DrawEditor(statusMessage);
		break;
	case CustomizeMode::AttackPicker:
		mode = characterEditor.DrawAttackPicker(statusMessage);
		break;
	default:
		DrawMainMenu();
		break;
	}
}

/// <summary>
/// ウィンドウサイズ変更後の ImGui 配置用サイズを更新する。
/// </summary>
/// <param name="newWidth">新しい幅。</param>
/// <param name="newHeight">新しい高さ。</param>
void CustomizeFlowController::OnResize(int newWidth, int newHeight)
{
	if (newWidth <= 0 || newHeight <= 0)
	{
		return;
	}

	width = newWidth;
	height = newHeight;
}

/// <summary>
/// CustomizeScene が保持する World を取得する。
/// </summary>
/// <returns>変更可能な World。</returns>
World& CustomizeFlowController::GetWorld()
{
	return world;
}

/// <summary>
/// CustomizeScene が保持する World を読み取り専用で取得する。
/// </summary>
/// <returns>読み取り専用の World。</returns>
const World& CustomizeFlowController::GetWorld() const
{
	return world;
}

/// <summary>
/// UI Cancel が押されたか確認する。
/// </summary>
/// <returns>Cancel が今フレーム押された場合は true。</returns>
bool CustomizeFlowController::WasCancelTriggered()
{
	for (int playerIndex = 0; playerIndex < Input::MaxPlayers; ++playerIndex)
	{
		const Input::InputActionState& cancel =
			Input::InputSystem::GetActionState(playerIndex, Input::InputActionId::Cancel);
		if (cancel.trigger)
		{
			return true;
		}
	}

	return false;
}

/// <summary>
/// タイトルシーンへの切り替えを予約する。
/// </summary>
void CustomizeFlowController::RequestTitleScene()
{
	SceneManager::GetInstance().RequestChangeScene(
		std::make_unique<TitleScene>(width, height));
}

/// <summary>
/// 現在のカスタマイズ階層から一段戻る。
/// </summary>
void CustomizeFlowController::NavigateBack()
{
	// 攻撃編集を抜けて攻撃スロット選択へ戻る場合だけ、次の技選択用に初期化する。
	if (mode == CustomizeMode::AttackEditor)
		previewController.ResetCamera();
	switch (mode)
	{
	case CustomizeMode::MainMenu:
		RequestTitleScene();
		break;
	case CustomizeMode::AttackCategorySelect:
		mode = CustomizeMode::MainMenu;
		break;
	case CustomizeMode::AttackSlotSelect:
		mode = CustomizeMode::AttackCategorySelect;
		break;
	case CustomizeMode::AttackEditor:
		editHistory.Reset();
		mode = CustomizeMode::AttackSlotSelect;
		break;
	case CustomizeMode::MotionEditor:
		editHistory.Reset();
		previewController.ResetCamera();
		mode = editingCommonMotion ? CustomizeMode::CommonMotionSelect : CustomizeMode::AttackSlotSelect;
		break;
	case CustomizeMode::CommonMotionSelect:
		mode = CustomizeMode::MainMenu;
		break;
	case CustomizeMode::CharacterSlotSelect:
		mode = CustomizeMode::MainMenu;
		break;
	case CustomizeMode::CharacterEditor:
		mode = CustomizeMode::CharacterSlotSelect;
		break;
	case CustomizeMode::AttackPicker:
		mode = CustomizeMode::CharacterEditor;
		break;
	default:
		mode = CustomizeMode::MainMenu;
		break;
	}
}

/// <summary>
/// カスタマイズシーン最初の選択画面を描画する。
/// </summary>
void CustomizeFlowController::DrawMainMenu()
{
	ImGui::SetNextWindowPos(ImVec2(40.0f, 40.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(360.0f, 180.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Customize"))
	{
		if (ImGui::Button("Attack Data Editor", ImVec2(220.0f, 32.0f)))
		{
			mode = CustomizeMode::AttackCategorySelect;
		}

		if (ImGui::Button("Character Editor", ImVec2(220.0f, 32.0f)))
		{
			characterEditor.RefreshSlotSummaries();
			mode = CustomizeMode::CharacterSlotSelect;
		}

		if (ImGui::Button("Common Motion Editor", ImVec2(220.0f, 32.0f)))
		{
			mode = CustomizeMode::CommonMotionSelect;
		}

		ImGui::Separator();
		if (ImGui::Button("Back To Title", ImVec2(220.0f, 28.0f)))
		{
			RequestTitleScene();
		}
	}
	ImGui::End();
}

/// <summary>
/// 作成する技カテゴリを選択する画面を描画する。
/// </summary>
void CustomizeFlowController::DrawAttackCategorySelect()
{
	ImGui::SetNextWindowPos(ImVec2(40.0f, 40.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(360.0f, 220.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Attack Category"))
	{
		for (int index = 0; index < static_cast<int>(std::size(CategoryLabels)); ++index)
		{
			ImGui::PushID(index);
			if (ImGui::Button(CategoryLabels[index], ImVec2(220.0f, 32.0f)))
			{
				selectedCategory = static_cast<CustomizeAttackCategory>(index);
				RefreshAttackSlotSummaries(selectedCategory);
				mode = CustomizeMode::AttackSlotSelect;
			}
			ImGui::PopID();
		}

		ImGui::Separator();
		if (ImGui::Button("Back", ImVec2(120.0f, 28.0f)))
		{
			NavigateBack();
		}
	}
	ImGui::End();
}

/// <summary>
/// 選択中カテゴリ内の技スロット一覧を描画する。
/// </summary>
void CustomizeFlowController::DrawAttackSlotSelect()
{
	ImGui::SetNextWindowPos(ImVec2(40.0f, 40.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(560.0f, 420.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Attack Slots"))
	{
		ImGui::Text("Category: %s", CategoryLabels[ToCategoryIndex(selectedCategory)]);
		ImGui::Separator();

		if (ImGui::Button("Refresh Slot Names", ImVec2(160.0f, 28.0f)))
		{
			RefreshAttackSlotSummaries(selectedCategory);
		}
		ImGui::Separator();

		const int slotCount = GetAttackSlotCount(selectedCategory);
		for (int slotIndex = 0; slotIndex < slotCount; ++slotIndex)
		{
			ImGui::PushID(slotIndex);
			const std::string label = BuildAttackSlotButtonLabel(selectedCategory, slotIndex);
			if (ImGui::Button(label.c_str(), ImVec2(126.0f, 52.0f)))
			{
				SelectAttackSlot(selectedCategory, slotIndex);
			}
			ImGui::PopID();

			if ((slotIndex + 1) % 4 != 0)
			{
				ImGui::SameLine();
			}
		}

		ImGui::Separator();
		if (ImGui::Button("Back", ImVec2(120.0f, 28.0f)))
		{
			NavigateBack();
		}
	}
	ImGui::End();
}

/// <summary>
/// 汎用モーションを選択し、MotionData 単体の編集画面へ入る。
/// </summary>
void CustomizeFlowController::DrawCommonMotionSelect()
{
	ImGui::SetNextWindowPos(ImVec2(40.0f, 40.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(420.0f, 460.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Common Motion Select"))
	{
		for (int slotIndex = 0; slotIndex < CommonMotionSlotCount; ++slotIndex)
		{
			ImGui::PushID(slotIndex);
			if (ImGui::Button(CommonMotionLabels[slotIndex], ImVec2(260.0f, 32.0f)))
			{
				SelectCommonMotionSlot(slotIndex);
			}
			ImGui::PopID();
		}

		ImGui::Separator();
		if (ImGui::Button("Back", ImVec2(120.0f, 28.0f)))
		{
			NavigateBack();
		}
	}
	ImGui::End();
}

/// <summary>
/// 本体左側に3Dプレビュー、右側に技パラメータ編集画面を描画する。
/// </summary>
/// <param name="renderer">本体のプレビュー矩形へ描画する Renderer。</param>
void CustomizeFlowController::DrawAttackEditor(Renderer& renderer)
{
	EnsureDraftAttackMotionDataId();
	if (!motionEditor.HasDraft()) LoadDraftMotionFromEditorId();
	ClampPreviewCurrentFrame();

	attackEditorView.DrawBackdrop(renderer, width, height);
	const RECT region = CustomizeAttackEditorView::PreviewRegion(width, height);
	RenderAttackPreview(renderer, &region);
	attackEditorView.Draw(renderer);
}

/// <summary>
/// 本体左側に3Dプレビュー、右側に固定したモーション編集画面を描画する。
/// </summary>
/// <param name="renderer">本体のプレビュー矩形へ描画する Renderer。</param>
void CustomizeFlowController::DrawMotionEditorScreen(Renderer& renderer)
{
    DrawAttackEditor(renderer);
}
/// <summary>共通編集UIの操作結果を受け、保存と画面遷移を振り分ける。</summary>
void CustomizeFlowController::UpdateAttackEditorControls()
{
	if (!motionEditor.HasDraft()) LoadDraftMotionFromEditorId();
	const auto before = CaptureEditSnapshot();
	const bool gizmoWasActive = previewController.IsGizmoEditing();
	const CustomizeAttackEditorAction action = attackEditorView.Update(attackEditor, previewController, width, height, statusMessage,
		motionEditor, mode == CustomizeMode::MotionEditor, editingCommonMotion, editHistory.CanUndo(), editHistory.CanRedo());
	const auto after = CaptureEditSnapshot();
	const auto interaction = attackEditorView.GetEditInteraction();
	const bool changed = !CustomizeEditHistory::Equal(before, after);
	if (changed || interaction.ongoing) previewController.Stop();
	// UIの空更新では、描画側で継続しているGizmoドラッグを確定しない。
	if (!gizmoWasActive || changed)
		editHistory.Observe(before, after, interaction.id, interaction.ongoing, interaction.wheel, EditHistoryTime());
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
		const bool found = action == CustomizeAttackEditorAction::Undo ? editHistory.Undo(restored) : editHistory.Redo(restored);
		statusMessage = found ? (action == CustomizeAttackEditorAction::Undo ? "操作を元に戻しました。" : "操作をやり直しました。") : "対象の操作はありません。";
		if (found) RestoreEditSnapshot(restored);
		return;
	}
	if (action == CustomizeAttackEditorAction::Save) { SaveDraftMotion(); return; }
	if (action == CustomizeAttackEditorAction::OpenAttackEditor)
	{
		mode = CustomizeMode::AttackEditor;
		return; // 編集画面同士の切替なので、下書きとカメラを維持する。
	}
	if (action == CustomizeAttackEditorAction::OpenMotionEditor)
	{
		if (!motionEditor.HasDraft()) LoadDraftMotionFromEditorId();
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
/// 指定カテゴリとスロット番号の技データを編集対象として読み込む。
/// </summary>
/// <param name="category">編集する技カテゴリ。</param>
/// <param name="slotIndex">カテゴリ内スロット番号。</param>
void CustomizeFlowController::SelectAttackSlot(CustomizeAttackCategory category, int slotIndex)
{
	editHistory.Reset();
	editingCommonMotion = false;
	editingCommonMotionId.clear();
	selectedCategory = category;
	statusMessage = attackEditor.SelectSlot(category, slotIndex);
	attackEditorView.Reset();
	motionEditor.ResetForAttackMotion();
	previewController.SetCurrentFrame(0);
	previewController.Stop();
	attackMovementKeyOffset = Vector2::Zero;
	mode = CustomizeMode::AttackEditor;
}

/// <summary>
/// 汎用モーションを MotionData 単体の編集対象として読み込む。
/// </summary>
/// <param name="slotIndex">CommonMotionLabels 配列上の番号。</param>
void CustomizeFlowController::SelectCommonMotionSlot(int slotIndex)
{
	editHistory.Reset();
	editingCommonMotion = true;
	selectedCommonMotionIndex = std::clamp(slotIndex, 0, CommonMotionSlotCount - 1);
	editingCommonMotionId = BuildCommonMotionDataId(selectedCommonMotionIndex);

	attackEditor.PrepareCommonMotionPreview(editingCommonMotionId);
	LoadDraftMotionFromEditorId();
	previewController.SetCurrentFrame(0);
	previewController.Stop();
	attackMovementKeyOffset = Vector2::Zero;
	attackEditorView.Reset();
	mode = CustomizeMode::MotionEditor;
}

/// <summary>
/// 編集中の技に、スロット単位で一意になる MotionData ID を割り当てる。
/// </summary>
void CustomizeFlowController::EnsureDraftAttackMotionDataId()
{
	if (editingCommonMotion)
	{
		return;
	}

	attackEditor.EnsureDraftMotionDataId();
}

/// <summary>
/// MotionData ID 入力欄の値から編集用 MotionData を読み込み、存在しなければ新規下書きを作る。
/// </summary>
void CustomizeFlowController::LoadDraftMotionFromEditorId()
{
	const std::string motionDataId = GetEditingMotionDataId();
	const std::string fallbackDisplayName = editingCommonMotion
		? CommonMotionLabels[selectedCommonMotionIndex]
		: motionDataId;
	const int totalFrames = editingCommonMotion ? 30 : GetPreviewTotalFrames();
	statusMessage = motionEditor.LoadDraft(motionDataId, fallbackDisplayName, totalFrames, editingCommonMotion);
	motionEditor.CopyEditorBuffers(GetPreviewActionFrame());
}

/// <summary>
/// 編集中の MotionData 下書きを JSON として保存する。
/// </summary>
void CustomizeFlowController::SaveDraftMotion()
{
    if (!motionEditor.HasDraft()) LoadDraftMotionFromEditorId();
    EnsureDraftAttackMotionDataId();
    const std::string motionId = GetEditingMotionDataId();
    const int total = GetPreviewTotalFrames();
    const std::string motionResult = motionEditor.SaveDraft(motionId, total,
        editingCommonMotion && motionEditor.GetDraft().looping);
    if (!motionResult.starts_with("Saved MotionData:"))
    {
        statusMessage = motionResult;
        return;
    }
    if (!editingCommonMotion)
    {
        const std::string attackResult = attackEditor.SaveDraft();
        if (!attackResult.starts_with("Saved:"))
        {
            statusMessage = "MotionData saved, but AttackData save failed. Retry saving both files.";
            return;
        }
    }
    statusMessage = editingCommonMotion ? "モーションを保存しました。" : "技とモーションを保存しました。";
}
void CustomizeFlowController::InitializePreview()
{
	previewController.Initialize();
}

/// <summary>
/// 技調整プレビュー用の RenderTexture を解放する。
/// </summary>
void CustomizeFlowController::ReleasePreview()
{
	previewController.Release();
}

/// <summary>
/// Play 中なら表示フレームを 1 つ進め、技プレビュー終端に到達したら停止する。
/// </summary>
void CustomizeFlowController::UpdatePreviewPlayback()
{
	const bool isPreviewMode = mode == CustomizeMode::AttackEditor || mode == CustomizeMode::MotionEditor;
	const bool advanced = previewController.UpdatePlayback(
		isPreviewMode,
		editingCommonMotion && motionEditor.GetDraft().looping,
		GetPreviewTotalFrames());
	if (!advanced)
	{
		return;
	}

	const int actionFrame = GetPreviewActionFrame();
	if (actionFrame >= 0)
	{
		const AttackData& draftAttack = attackEditor.GetDraft();
		motionEditor.RefreshFrameEditValues(actionFrame);
		attackMovementKeyOffset = motionEditor.GetAttackMovementOffsetAtFrame(draftAttack, actionFrame);
	}
}

/// <summary>
/// プレビュー用カメラでモデルを描画する。本体矩形が渡された場合はバックバッファへ直接描画する。
/// </summary>
/// <param name="renderer">描画に使う Renderer。</param>
/// <param name="region">本体の描画矩形。nullptr の場合は従来の RenderTexture。</param>
void CustomizeFlowController::RenderAttackPreview(Renderer& renderer, const RECT* region)
{
	const auto beforeEdit = CaptureEditSnapshot();
	const int actionFrame = GetPreviewActionFrame();
	const bool motionMode = mode == CustomizeMode::MotionEditor;
	const bool movementMode = motionMode && attackEditorView.IsMovementSelected();
	previewController.rootMovementGizmoEnabled = movementMode && !previewController.IsPlaying()
		&& (editingCommonMotion ? motionEditor.HasRootOffsetKeyframe(actionFrame)
			: motionEditor.HasAttackMovementKeyframe(attackEditor.GetDraft(), actionFrame));
	if (motionMode) previewController.editorToolbarExclusions = attackEditorView.PreviewToolbarExclusions();
	else previewController.editorToolbarExclusions.clear();
	previewController.editorInputBlocked = false; // プリセットはパネル内展開なのでプレビュー全体を遮断しない。
	previewController.showJointMarkers = attackEditorView.AreMarkersVisible();
	previewController.selectedMarkerParts = motionEditor.GetSelectedBodyParts();
	const bool rotationGizmoEnabled =
		mode == CustomizeMode::MotionEditor
		&& region != nullptr
		&& !previewController.IsPlaying()
		&& !movementMode
		&& motionEditor.HasMotionKeyframe(actionFrame);

	previewController.Render(
		renderer,
		region,
		attackEditor.GetDraft(),
		motionEditor.GetDraft(),
		editingCommonMotion,
		motionEditor.HasDraft(),
		GetEditingMotionDataId(),
		motionEditor.GetSelectedBoneIndex(),
		rotationGizmoEnabled);
	if (motionMode)
	{
		const int picked = previewController.ConsumePickedBodyPartIndex();
		if (!movementMode && picked != -1) motionEditor.SelectBodyPart(previewController, picked == -2 ? -1 : picked, actionFrame);
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
			else motionEditor.UpdateAttackMovementKeyframe(attackEditor.GetDraft(), actionFrame, Vector2(offset.x, offset.y));
		}
	}

	int gizmoBodyPartIndex = -1;
	int gizmoAxisIndex = -1;
	Quaternion gizmoLocalRotation = Quaternion::Identity;
	if (previewController.ConsumeGizmoRotationEdit(
		gizmoBodyPartIndex,
		gizmoAxisIndex,
		gizmoLocalRotation))
	{
		statusMessage = motionEditor.ApplyGizmoRotationEdit(
			gizmoBodyPartIndex,
			gizmoAxisIndex,
			actionFrame,
			GetPreviewTotalFrames(),
			gizmoLocalRotation);
	}
	const auto afterEdit = CaptureEditSnapshot();
	if (previewController.IsGizmoEditing() || editHistory.IsPending("gizmo") || !CustomizeEditHistory::Equal(beforeEdit, afterEdit))
		editHistory.Observe(beforeEdit, afterEdit, "gizmo", previewController.IsGizmoEditing(), false, EditHistoryTime());
}

CustomizeEditSnapshot CustomizeFlowController::CaptureEditSnapshot()
{
	CustomizeEditSnapshot snapshot;
	snapshot.attack = attackEditor.GetEditState();
	snapshot.motion = motionEditor.GetDraft();
	snapshot.motion.displayName = motionEditor.GetDisplayNameBuffer().data();
	snapshot.commonMotion = editingCommonMotion;
	snapshot.previewFrame = previewController.GetCurrentFrame();
	return snapshot;
}

void CustomizeFlowController::RestoreEditSnapshot(const CustomizeEditSnapshot& snapshot)
{
	previewController.Stop();
	previewController.CancelGizmoEditing();
	motionEditor.EndIkEdit(previewController);
	if (!editingCommonMotion) attackEditor.ApplyEditState(snapshot.attack);
	previewController.SetCurrentFrame(snapshot.previewFrame);
	motionEditor.RestoreDraft(snapshot.motion, snapshot.previewFrame - 1);
	ClampPreviewCurrentFrame();
	motionEditor.RefreshFrameEditValues(GetPreviewActionFrame());
	attackMovementKeyOffset = motionEditor.GetAttackMovementOffsetAtFrame(attackEditor.GetDraft(), GetPreviewActionFrame());
	attackEditorView.ResetAfterHistory();
	// 描画用の値参照も復元直後に作り直す。空入力を使い、Undoボタンを二度実行しない。
	attackEditorView.Update(attackEditor, previewController, width, height, statusMessage,
		motionEditor, mode == CustomizeMode::MotionEditor, editingCommonMotion, editHistory.CanUndo(), editHistory.CanRedo());
}

/// <summary>
/// 現在フレームを、0F の Idle を含むプレビュー表示範囲内へ収める。
/// </summary>
void CustomizeFlowController::ClampPreviewCurrentFrame()
{
	previewController.ClampCurrentFrame(GetPreviewTotalFrames());
}

/// <summary>
/// AttackFrameData の新しい発生フレーム定義から、内部処理上の攻撃総フレーム数を取得する。
/// </summary>
/// <returns>最低 1F を保証した総フレーム数。プレビュー表示では 0F Idle を含めて 0..この値まで表示する。</returns>
int CustomizeFlowController::GetPreviewTotalFrames() const
{
	if (editingCommonMotion)
	{
		return std::max(1, motionEditor.GetDraft().totalFrames);
	}

	const AttackData& draftAttack = attackEditor.GetDraft();
	return GetAttackTotalFrames(draftAttack.frame);
}

/// <summary>
/// プレビュー表示フレームを、内部の PlayerActionState::actionFrame 相当へ変換する。
/// </summary>
/// <returns>0F Idle は -1、1F 以降は 0 始まりの内部 actionFrame。</returns>
int CustomizeFlowController::GetPreviewActionFrame() const
{
	return previewController.GetActionFrame();
}

/// <summary>
/// 現在のプレビュー表示フレームが Idle、前隙、攻撃判定中、後隙のどこにいるかを返す。
/// </summary>
/// <returns>現在フェーズの表示名。</returns>
const char* CustomizeFlowController::GetPreviewPhaseText() const
{
	if (previewController.GetCurrentFrame() <= 0)
	{
		return "Idle";
	}
	if (editingCommonMotion)
	{
		return motionEditor.GetDraft().looping ? "Common Loop Motion" : "Common Motion";
	}

	const int actionFrame = GetPreviewActionFrame();
	const AttackData& draftAttack = attackEditor.GetDraft();
	const int activeStartFrame = GetAttackActiveStartFrame(draftAttack.frame);
	const int activeEndFrame = GetAttackActiveEndFrameExclusive(draftAttack.frame);
	const int totalFrames = GetPreviewTotalFrames();

	if (actionFrame < activeStartFrame)
	{
		return "Startup";
	}
	if (IsAttackFrameActive(draftAttack.frame, actionFrame))
	{
		return "Active";
	}
	if (actionFrame >= activeEndFrame && actionFrame < totalFrames)
	{
		return "Recovery";
	}

	return "End";
}

/// <summary>
/// 現在の編集対象から、読み書きする MotionData ID を取得する。
/// </summary>
/// <returns>Common 編集中は Common ID、Attack 編集中は AttackData の motionDataId。</returns>
std::string CustomizeFlowController::GetEditingMotionDataId() const
{
	if (editingCommonMotion)
	{
		return editingCommonMotionId.empty() ? std::string(attackEditor.GetMotionDataIdBuffer().data()) : editingCommonMotionId;
	}

	return attackEditor.GetDraft().motionDataId;
}

/// <summary>
/// 技カテゴリごとのスロット数を取得する。
/// </summary>
/// <param name="category">確認する技カテゴリ。</param>
/// <returns>対象カテゴリのスロット数。</returns>
int CustomizeFlowController::GetAttackSlotCount(CustomizeAttackCategory category) const
{
	return attackEditor.GetSlotCount(category);
}

/// <summary>
/// 選択カテゴリのスロット JSON を確認し、一覧表示用の保存済み名を更新する。
/// </summary>
/// <param name="category">更新する技カテゴリ。</param>
void CustomizeFlowController::RefreshAttackSlotSummaries(CustomizeAttackCategory category)
{
	attackEditor.RefreshSlotSummaries(category);
}

/// <summary>
/// スロット番号と保存済み技名をまとめた、ImGui Button 用ラベルを作る。
/// </summary>
/// <param name="category">表示する技カテゴリ。</param>
/// <param name="slotIndex">カテゴリ内スロット番号。</param>
/// <returns>表示名と ImGui ID を含むボタンラベル。</returns>
std::string CustomizeFlowController::BuildAttackSlotButtonLabel(CustomizeAttackCategory category, int slotIndex) const
{
	return attackEditor.BuildSlotButtonLabel(category, slotIndex);
}

/// <summary>
/// カテゴリとスロット番号から、assets/AttackData 配下の保存 ID を作る。
/// </summary>
/// <param name="category">保存カテゴリ。</param>
/// <param name="slotIndex">カテゴリ内スロット番号。</param>
/// <returns>拡張子なしの AttackData ID。</returns>
std::string CustomizeFlowController::BuildAttackDataId(CustomizeAttackCategory category, int slotIndex) const
{
	return attackEditor.BuildAttackDataId(category, slotIndex);
}

/// <summary>
/// カテゴリとスロット番号から、assets/MotionData 配下の保存 ID を作る。
/// </summary>
/// <param name="category">保存カテゴリ。</param>
/// <param name="slotIndex">カテゴリ内スロット番号。</param>
/// <returns>拡張子なしの MotionData ID。</returns>
std::string CustomizeFlowController::BuildMotionDataId(CustomizeAttackCategory category, int slotIndex) const
{
	return attackEditor.BuildMotionDataId(category, slotIndex);
}

/// <summary>
/// 汎用モーション番号から、assets/MotionData 配下の保存 ID を作る。
/// </summary>
/// <param name="slotIndex">CommonMotionLabels 配列上の番号。</param>
/// <returns>Common/Idle のような MotionData ID。</returns>
std::string CustomizeFlowController::BuildCommonMotionDataId(int slotIndex) const
{
	const int clampedSlotIndex = std::clamp(slotIndex, 0, CommonMotionSlotCount - 1);
	return std::string("Common/") + CommonMotionLabels[clampedSlotIndex];
}

/// <summary>
/// draftMotion の表示名と編集対象ボーン名を ImGui 入力用固定バッファへコピーする。
/// </summary>
void CustomizeFlowController::CopyMotionEditorBuffers()
{
	motionEditor.CopyEditorBuffers(GetPreviewActionFrame());
}


