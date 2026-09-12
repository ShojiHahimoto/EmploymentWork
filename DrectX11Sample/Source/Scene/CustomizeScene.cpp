#include "Scene/CustomizeScene.h"

#include "Data/AttackDataSaver.h"
#include "Input/InputSystem.h"
#include "Input/InputTypes.h"
#include "Scene/SceneManager.h"
#include "Scene/TitleScene.h"
#include "System/imgui-docking/imgui.h"

#include <algorithm>
#include <sstream>

using namespace DirectX::SimpleMath;

namespace
{
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
	struct CustomizePreviewLayout
	{
		int splitX = 1;
		int previewBottom = 1;
		RECT previewRegion = {};
		ImVec2 origin = ImVec2(0.0f, 0.0f);
		ImGuiID viewportId = 0;
		ImGuiWindowFlags fixedFlags = 0;
	};

	/// <summary>
	/// カスタマイズ画面共通の左プレビュー・右編集欄レイアウトを計算する。
	/// </summary>
	/// <param name="width">現在のクライアント幅。</param>
	/// <param name="height">現在のクライアント高さ。</param>
	/// <returns>プレビュー矩形と固定 ImGui ウィンドウ用情報。</returns>
	CustomizePreviewLayout CreateCustomizePreviewLayout(int width, int height)
	{
		CustomizePreviewLayout layout;
		layout.splitX = std::max(1, width / 2);
		layout.previewBottom = std::max(1, height - 140);
		layout.previewRegion = { 0, 0, layout.splitX, layout.previewBottom };

		const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
		layout.origin = mainViewport->Pos;
		layout.viewportId = mainViewport->ID;
		layout.fixedFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings;
		return layout;
	}

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
/// CustomizeScene を現在の描画サイズで初期化する。
/// </summary>
/// <param name="initialWidth">初期ウィンドウ幅。</param>
/// <param name="initialHeight">初期ウィンドウ高さ。</param>
CustomizeScene::CustomizeScene(int initialWidth, int initialHeight)
	: width(initialWidth)
	, height(initialHeight)
{
}

/// <summary>
/// カスタマイズ用入力マップに切り替え、初期メニューへ戻す。
/// </summary>
void CustomizeScene::Enter()
{
	Input::InputSystem::SetActionMap(Input::InputActionMapId::UI);
	mode = CustomizeMode::MainMenu;
	statusMessage.clear();
	InitializePreview();
}

/// <summary>
/// カスタマイズシーンが保持する一時 World を破棄する。
/// </summary>
void CustomizeScene::Exit()
{
	ReleasePreview();
	world.Clear();
}

/// <summary>
/// キャンセル入力があれば、現在の編集階層から一つ戻る。
/// </summary>
void CustomizeScene::RunSystems()
{
	UpdatePreviewPlayback();

	if (WasCancelTriggered())
	{
		NavigateBack();
	}
}

/// <summary>
/// カスタマイズ用の ImGui 画面を描画する。
/// </summary>
/// <param name="renderer">現段階では未使用。後でプレビュー描画に使う。</param>
void CustomizeScene::Draw(Renderer& renderer)
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
void CustomizeScene::OnResize(int newWidth, int newHeight)
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
World& CustomizeScene::GetWorld()
{
	return world;
}

/// <summary>
/// CustomizeScene が保持する World を読み取り専用で取得する。
/// </summary>
/// <returns>読み取り専用の World。</returns>
const World& CustomizeScene::GetWorld() const
{
	return world;
}

/// <summary>
/// UI Cancel が押されたか確認する。
/// </summary>
/// <returns>Cancel が今フレーム押された場合は true。</returns>
bool CustomizeScene::WasCancelTriggered()
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
void CustomizeScene::RequestTitleScene()
{
	SceneManager::GetInstance().RequestChangeScene(
		std::make_unique<TitleScene>(width, height));
}

/// <summary>
/// 現在のカスタマイズ階層から一段戻る。
/// </summary>
void CustomizeScene::NavigateBack()
{
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
		mode = CustomizeMode::AttackSlotSelect;
		break;
	case CustomizeMode::MotionEditor:
		mode = editingCommonMotion ? CustomizeMode::CommonMotionSelect : CustomizeMode::AttackEditor;
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
void CustomizeScene::DrawMainMenu()
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
void CustomizeScene::DrawAttackCategorySelect()
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
void CustomizeScene::DrawAttackSlotSelect()
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
void CustomizeScene::DrawCommonMotionSelect()
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
void CustomizeScene::DrawAttackEditor(Renderer& renderer)
{
	EnsureDraftAttackMotionDataId();
	ClampPreviewCurrentFrame();

	// モーション作成画面と同じ表示規則にして、左側の本体ウィンドウ上に直接プレビューする。
	const CustomizePreviewLayout layout = CreateCustomizePreviewLayout(width, height);

	ImGui::SetNextWindowViewport(layout.viewportId);
	ImGui::SetNextWindowPos(ImVec2(layout.origin.x, layout.origin.y + layout.previewBottom), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(static_cast<float>(layout.splitX), static_cast<float>(std::max(1, height - layout.previewBottom))), ImGuiCond_Always);
	if (ImGui::Begin("Attack Playback", nullptr, layout.fixedFlags))
	{
		ImGui::Text("Editing: %s", attackEditor.GetEditingAttackDataId().c_str());
		ImGui::Text("Frame: %d / %d", previewController.GetCurrentFrame(), GetPreviewTotalFrames());
		ImGui::Text("Phase: %s", GetPreviewPhaseText());
		DrawPreviewPlaybackControls();
	}
	ImGui::End();

	ImGui::SetNextWindowViewport(layout.viewportId);
	ImGui::SetNextWindowPos(ImVec2(layout.origin.x + layout.splitX, layout.origin.y), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(static_cast<float>(std::max(1, width - layout.splitX)), static_cast<float>(std::max(1, height))), ImGuiCond_Always);
	if (ImGui::Begin("Attack Parameters", nullptr, layout.fixedFlags))
	{
		DrawAttackEditorControls();
	}
	ImGui::End();

	RenderAttackPreview(renderer, &layout.previewRegion);
}

/// <summary>
/// 本体左側に3Dプレビュー、右側に固定したモーション編集画面を描画する。
/// </summary>
/// <param name="renderer">本体のプレビュー矩形へ描画する Renderer。</param>
void CustomizeScene::DrawMotionEditorScreen(Renderer& renderer)
{
	if (!editingCommonMotion)
	{
		EnsureDraftAttackMotionDataId();
	}
	if (!motionEditor.HasDraft())
	{
		LoadDraftMotionFromEditorId();
	}

	ClampPreviewCurrentFrame();
	// 左側は本体バックバッファ。ImGui は操作欄だけに限定し、3D領域を覆わない。
	const CustomizePreviewLayout layout = CreateCustomizePreviewLayout(width, height);
	ImGui::SetNextWindowViewport(layout.viewportId);
	ImGui::SetNextWindowPos(ImVec2(layout.origin.x, layout.origin.y + layout.previewBottom), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(static_cast<float>(layout.splitX), static_cast<float>(std::max(1, height - layout.previewBottom))), ImGuiCond_Always);
	if (ImGui::Begin("Motion Playback", nullptr, layout.fixedFlags))
	{
		ImGui::Text("Frame: %d / %d", previewController.GetCurrentFrame(), GetPreviewTotalFrames());
		ImGui::Text("Phase: %s", GetPreviewPhaseText());
		DrawPreviewPlaybackControls();
	}
	ImGui::End();
	ImGui::SetNextWindowViewport(layout.viewportId);
	ImGui::SetNextWindowPos(ImVec2(layout.origin.x + layout.splitX, layout.origin.y), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(static_cast<float>(std::max(1, width - layout.splitX)), static_cast<float>(std::max(1, height))), ImGuiCond_Always);
	if (ImGui::Begin("Motion Editor", nullptr, layout.fixedFlags))
	{
		if (editingCommonMotion)
		{
			ImGui::Text("Common Motion: %s", CommonMotionLabels[selectedCommonMotionIndex]);
		}
		else
		{
			ImGui::Text("Attack: %s", attackEditor.GetEditingAttackDataId().c_str());
		}
		ImGui::Text("MotionData ID: %s", attackEditor.GetMotionDataIdBuffer().data());
		ImGui::Separator();
		ImGui::Text("Preview Camera");
		ImGui::SliderFloat("Camera Yaw", &previewController.GetCameraYawDegrees(), -180.0f, 180.0f);
		ImGui::SliderFloat("Camera Pitch", &previewController.GetCameraPitchDegrees(), -45.0f, 65.0f);
		ImGui::SliderFloat("Camera Distance", &previewController.GetCameraDistance(), 5.0f, 30.0f);
		if (motionEditor.DrawEditor(
			attackEditor.GetDraft(),
			previewController,
			editingCommonMotion,
			GetPreviewTotalFrames(),
			statusMessage,
			attackMovementKeyOffset))
		{
			SaveDraftMotion();
		}

		ImGui::Separator();
		if (ImGui::Button(editingCommonMotion ? "Back To Common Motion Select" : "Back To Attack Editor", ImVec2(240.0f, 30.0f)))
		{
			mode = editingCommonMotion ? CustomizeMode::CommonMotionSelect : CustomizeMode::AttackEditor;
		}
	}
	ImGui::End();
	// 数値変更・フレーム移動と同じ描画フレームに姿勢を反映する。
	RenderAttackPreview(renderer, &layout.previewRegion);
}

/// <summary>
/// プレビュー再生と1フレーム送り・戻しの共通操作を描画する。
/// </summary>
void CustomizeScene::DrawPreviewPlaybackControls()
{
	if (ImGui::Button("Play", ImVec2(72.0f, 28.0f)))
	{
		previewController.Play(GetPreviewTotalFrames());
	}
	ImGui::SameLine();
	if (ImGui::Button("Stop", ImVec2(72.0f, 28.0f)))
	{
		previewController.Stop();
	}
	if (ImGui::GetContentRegionAvail().x >= 320.0f)
	{
		ImGui::SameLine();
	}
	if (ImGui::Button("< 1F", ImVec2(72.0f, 28.0f)))
	{
		StepPreviewFrame(-1);
	}
	ImGui::SameLine();
	if (ImGui::Button("1F >", ImVec2(72.0f, 28.0f)))
	{
		StepPreviewFrame(1);
	}
}

/// <summary>
/// 技パラメータを編集する右側 ImGui ウィンドウを描画する。
/// </summary>
void CustomizeScene::DrawAttackEditorWindow()
{
	ImGui::SetNextWindowPos(ImVec2(static_cast<float>(width) * 0.5f, 20.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(static_cast<float>(width) * 0.5f - 20.0f, static_cast<float>(height) - 40.0f), ImGuiCond_Always);
	if (ImGui::Begin("Attack Parameters"))
	{
		DrawAttackEditorControls();
	}
	ImGui::End();
}

/// <summary>
/// 技パラメータ編集の中身を描画する。
/// </summary>
void CustomizeScene::DrawAttackEditorControls()
{
	const CustomizeAttackEditorAction action = attackEditor.DrawEditorControls(GetPreviewTotalFrames(), statusMessage);
	if (action == CustomizeAttackEditorAction::OpenMotionEditor)
	{
		LoadDraftMotionFromEditorId();
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
void CustomizeScene::SelectAttackSlot(CustomizeAttackCategory category, int slotIndex)
{
	editingCommonMotion = false;
	editingCommonMotionId.clear();
	selectedCategory = category;
	statusMessage = attackEditor.SelectSlot(category, slotIndex);
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
void CustomizeScene::SelectCommonMotionSlot(int slotIndex)
{
	editingCommonMotion = true;
	selectedCommonMotionIndex = std::clamp(slotIndex, 0, CommonMotionSlotCount - 1);
	editingCommonMotionId = BuildCommonMotionDataId(selectedCommonMotionIndex);

	attackEditor.PrepareCommonMotionPreview(editingCommonMotionId);
	LoadDraftMotionFromEditorId();
	previewController.SetCurrentFrame(0);
	previewController.Stop();
	attackMovementKeyOffset = Vector2::Zero;
	mode = CustomizeMode::MotionEditor;
}

/// <summary>
/// 編集中の draft を JSON として保存する。
/// </summary>
void CustomizeScene::SaveDraftAttack()
{
	statusMessage = attackEditor.SaveDraft();
}

/// <summary>
/// ImGui 入力欄の内容を draftAttack に反映する。
/// </summary>
void CustomizeScene::SyncDraftFromEditor()
{
	if (editingCommonMotion)
	{
		return;
	}

	attackEditor.SyncDraftFromEditor();
}

/// <summary>
/// 編集中の技に、スロット単位で一意になる MotionData ID を割り当てる。
/// </summary>
void CustomizeScene::EnsureDraftAttackMotionDataId()
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
void CustomizeScene::LoadDraftMotionFromEditorId()
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
void CustomizeScene::SaveDraftMotion()
{
	if (!motionEditor.HasDraft())
	{
		statusMessage = "No MotionData draft.";
		return;
	}

	if (editingCommonMotion)
	{
		editingCommonMotionId = attackEditor.GetMotionDataIdBuffer().data();
	}
	else
	{
		AttackData& draftAttack = attackEditor.GetDraft();
		draftAttack.motionDataId = attackEditor.GetMotionDataIdBuffer().data();
		EnsureDraftAttackMotionDataId();
	}

	const std::string motionDataId = editingCommonMotion
		? editingCommonMotionId
		: attackEditor.GetDraft().motionDataId;
	const int totalFrames = editingCommonMotion
		? motionEditor.GetDraft().totalFrames
		: GetPreviewTotalFrames();
	statusMessage = motionEditor.SaveDraft(motionDataId, totalFrames, editingCommonMotion && motionEditor.GetDraft().looping);
	if (statusMessage.rfind("Saved MotionData:", 0) == 0)
	{
		if (!editingCommonMotion)
		{
			SyncDraftFromEditor();
			AttackDataSaver::SaveAttackData(attackEditor.GetEditingAttackDataId(), attackEditor.GetDraft());
		}
	}
}

/// <summary>
/// 技調整プレビュー用のモデル、カメラ、RenderTexture を初期化する。
/// </summary>
void CustomizeScene::InitializePreview()
{
	previewController.Initialize();
}

/// <summary>
/// 技調整プレビュー用の RenderTexture を解放する。
/// </summary>
void CustomizeScene::ReleasePreview()
{
	previewController.Release();
}

/// <summary>
/// Play 中なら表示フレームを 1 つ進め、技プレビュー終端に到達したら停止する。
/// </summary>
void CustomizeScene::UpdatePreviewPlayback()
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
void CustomizeScene::RenderAttackPreview(Renderer& renderer, const RECT* region)
{
	previewController.Render(
		renderer,
		region,
		attackEditor.GetDraft(),
		motionEditor.GetDraft(),
		editingCommonMotion,
		motionEditor.HasDraft(),
		GetEditingMotionDataId(),
		motionEditor.GetSelectedBoneIndex());
}

/// <summary>
/// 現在フレームを、0F の Idle を含むプレビュー表示範囲内へ収める。
/// </summary>
void CustomizeScene::ClampPreviewCurrentFrame()
{
	previewController.ClampCurrentFrame(GetPreviewTotalFrames());
}

/// <summary>
/// プレビュー再生を止め、指定フレーム数だけ手動で進める。
/// </summary>
/// <param name="frameDelta">進めるフレーム数。負数なら戻す。</param>
void CustomizeScene::StepPreviewFrame(int frameDelta)
{
	previewController.StepFrame(frameDelta, GetPreviewTotalFrames());

	const int actionFrame = GetPreviewActionFrame();
	const AttackData& draftAttack = attackEditor.GetDraft();
	motionEditor.RefreshFrameEditValues(actionFrame);
	attackMovementKeyOffset = actionFrame >= 0
		? motionEditor.GetAttackMovementOffsetAtFrame(draftAttack, actionFrame)
		: Vector2::Zero;
}

/// <summary>
/// AttackFrameData の新しい発生フレーム定義から、内部処理上の攻撃総フレーム数を取得する。
/// </summary>
/// <returns>最低 1F を保証した総フレーム数。プレビュー表示では 0F Idle を含めて 0..この値まで表示する。</returns>
int CustomizeScene::GetPreviewTotalFrames() const
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
int CustomizeScene::GetPreviewActionFrame() const
{
	return previewController.GetActionFrame();
}

/// <summary>
/// 現在のプレビュー表示フレームが Idle、前隙、攻撃判定中、後隙のどこにいるかを返す。
/// </summary>
/// <returns>現在フェーズの表示名。</returns>
const char* CustomizeScene::GetPreviewPhaseText() const
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
std::string CustomizeScene::GetEditingMotionDataId() const
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
int CustomizeScene::GetAttackSlotCount(CustomizeAttackCategory category) const
{
	return attackEditor.GetSlotCount(category);
}

/// <summary>
/// 選択カテゴリのスロット JSON を確認し、一覧表示用の保存済み名を更新する。
/// </summary>
/// <param name="category">更新する技カテゴリ。</param>
void CustomizeScene::RefreshAttackSlotSummaries(CustomizeAttackCategory category)
{
	attackEditor.RefreshSlotSummaries(category);
}

/// <summary>
/// スロット番号と保存済み技名をまとめた、ImGui Button 用ラベルを作る。
/// </summary>
/// <param name="category">表示する技カテゴリ。</param>
/// <param name="slotIndex">カテゴリ内スロット番号。</param>
/// <returns>表示名と ImGui ID を含むボタンラベル。</returns>
std::string CustomizeScene::BuildAttackSlotButtonLabel(CustomizeAttackCategory category, int slotIndex) const
{
	return attackEditor.BuildSlotButtonLabel(category, slotIndex);
}

/// <summary>
/// カテゴリとスロット番号から、assets/AttackData 配下の保存 ID を作る。
/// </summary>
/// <param name="category">保存カテゴリ。</param>
/// <param name="slotIndex">カテゴリ内スロット番号。</param>
/// <returns>拡張子なしの AttackData ID。</returns>
std::string CustomizeScene::BuildAttackDataId(CustomizeAttackCategory category, int slotIndex) const
{
	return attackEditor.BuildAttackDataId(category, slotIndex);
}

/// <summary>
/// カテゴリとスロット番号から、assets/MotionData 配下の保存 ID を作る。
/// </summary>
/// <param name="category">保存カテゴリ。</param>
/// <param name="slotIndex">カテゴリ内スロット番号。</param>
/// <returns>拡張子なしの MotionData ID。</returns>
std::string CustomizeScene::BuildMotionDataId(CustomizeAttackCategory category, int slotIndex) const
{
	return attackEditor.BuildMotionDataId(category, slotIndex);
}

/// <summary>
/// 汎用モーション番号から、assets/MotionData 配下の保存 ID を作る。
/// </summary>
/// <param name="slotIndex">CommonMotionLabels 配列上の番号。</param>
/// <returns>Common/Idle のような MotionData ID。</returns>
std::string CustomizeScene::BuildCommonMotionDataId(int slotIndex) const
{
	const int clampedSlotIndex = std::clamp(slotIndex, 0, CommonMotionSlotCount - 1);
	return std::string("Common/") + CommonMotionLabels[clampedSlotIndex];
}

/// <summary>
/// draftMotion の表示名と編集対象ボーン名を ImGui 入力用固定バッファへコピーする。
/// </summary>
void CustomizeScene::CopyMotionEditorBuffers()
{
	motionEditor.CopyEditorBuffers(GetPreviewActionFrame());
}
