#include "Scene/CustomizeScene.h"

#include "Data/AttackDataSaver.h"
#include "Data/CharacterDataLoader.h"
#include "Input/InputSystem.h"
#include "Input/InputTypes.h"
#include "Resource/ModelResource.h"
#include "Scene/SceneManager.h"
#include "Scene/TitleScene.h"
#include "System/CameraSystem.h"
#include "System/Debugger.h"
#include "System/TransformSystem.h"
#include "System/imgui-docking/imgui.h"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iterator>
#include <memory>
#include <sstream>

using namespace DirectX::SimpleMath;

namespace
{
	constexpr const char* AttackDataRootPath = "assets/AttackData";
	constexpr const char* PreviewModelKey = "CustomizePreviewPlayer";
	constexpr const char* PreviewModelPath = "assets/model/DebugPlayer/man.fbx";
	constexpr float PreviewBoxDepth = 0.08f;
	constexpr const char* CategoryLabels[] = { "Ground", "Air", "Special" };
	constexpr AttackUsableState UsableStateValues[] = {
		AttackUsableState::Ground,
		AttackUsableState::Air,
		AttackUsableState::Both
	};
	constexpr const char* UsableStateLabels[] = { "Ground", "Air", "Both" };
	constexpr HitReactionType HitReactionValues[] = {
		HitReactionType::Normal,
		HitReactionType::Down,
		HitReactionType::Burst,
		HitReactionType::HardBurst
	};
	constexpr const char* HitReactionLabels[] = { "Normal", "Down", "Burst", "HardBurst" };
	constexpr AttackCommandId CommandValues[] = {
		AttackCommandId::None,
		AttackCommandId::Hadouken,
		AttackCommandId::Shoryuu,
		AttackCommandId::Yoga,
		AttackCommandId::ReverseYoga,
		AttackCommandId::FullRotate
	};
	constexpr const char* CommandLabels[] = {
		"None",
		"Hadouken",
		"Shoryuu",
		"Yoga",
		"ReverseYoga",
		"FullRotate"
	};

	/// <summary>
	/// 編集対象の AttackData ID から JSON ファイルのパスを作る。
	/// </summary>
	/// <param name="attackDataId">assets/AttackData から見た拡張子なしの技 ID。</param>
	/// <returns>読み書き対象の JSON ファイルパス。</returns>
	std::filesystem::path BuildAttackDataPath(const std::string& attackDataId)
	{
		return std::filesystem::path(AttackDataRootPath) / (attackDataId + ".json");
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

	/// <summary>
	/// AttackUsableState の現在値が Combo 配列の何番目かを取得する。
	/// </summary>
	/// <param name="value">検索する AttackUsableState。</param>
	/// <returns>Combo 用 index。</returns>
	int FindUsableStateIndex(AttackUsableState value)
	{
		for (int index = 0; index < static_cast<int>(std::size(UsableStateValues)); ++index)
		{
			if (UsableStateValues[index] == value)
			{
				return index;
			}
		}

		return 0;
	}

	/// <summary>
	/// HitReactionType の現在値が Combo 配列の何番目かを取得する。
	/// </summary>
	/// <param name="value">検索する HitReactionType。</param>
	/// <returns>Combo 用 index。</returns>
	int FindHitReactionIndex(HitReactionType value)
	{
		for (int index = 0; index < static_cast<int>(std::size(HitReactionValues)); ++index)
		{
			if (HitReactionValues[index] == value)
			{
				return index;
			}
		}

		return 0;
	}

	/// <summary>
	/// AttackCommandId の現在値が Combo 配列の何番目かを取得する。
	/// </summary>
	/// <param name="value">検索する AttackCommandId。</param>
	/// <returns>Combo 用 index。</returns>
	int FindCommandIndex(AttackCommandId value)
	{
		for (int index = 0; index < static_cast<int>(std::size(CommandValues)); ++index)
		{
			if (CommandValues[index] == value)
			{
				return index;
			}
		}

		return 0;
	}

	/// <summary>
	/// ImGui の入力後に、フレームやダメージが負数にならないよう補正する。
	/// </summary>
	/// <param name="attackData">補正する AttackData。</param>
	void ClampAttackDataValues(AttackData& attackData)
	{
		attackData.damage = std::max(0, attackData.damage);
		attackData.hitstunFrames = std::max(0, attackData.hitstunFrames);
		attackData.guardstunFrames = std::max(0, attackData.guardstunFrames);
		attackData.frame.startup = std::max(0, attackData.frame.startup);
		attackData.frame.active = std::max(0, attackData.frame.active);
		attackData.frame.recovery = std::max(0, attackData.frame.recovery);
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

		ImGui::BeginDisabled();
		ImGui::Button("Character Editor", ImVec2(220.0f, 32.0f));
		ImGui::EndDisabled();

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

		const int slotCount = GetAttackSlotCount(selectedCategory);
		for (int slotIndex = 0; slotIndex < slotCount; ++slotIndex)
		{
			ImGui::PushID(slotIndex);
			std::ostringstream label;
			label << "Slot " << std::setw(2) << std::setfill('0') << slotIndex;
			if (ImGui::Button(label.str().c_str(), ImVec2(120.0f, 28.0f)))
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
/// 技調整画面全体を描画する。
/// </summary>
/// <param name="renderer">プレビュー RenderTexture と ImGui 表示に使う Renderer。</param>
void CustomizeScene::DrawAttackEditor(Renderer& renderer)
{
	ClampPreviewCurrentFrame();
	RenderAttackPreview(renderer);
	DrawAttackPreviewWindow(renderer);
	DrawAttackEditorWindow();
}

/// <summary>
/// 左側の技プレビューウィンドウを描画し、再生ボタンで previewCurrentFrame を操作する。
/// </summary>
/// <param name="renderer">RenderTexture 表示のために受け取る Renderer。描画本体は RenderAttackPreview 側で行う。</param>
void CustomizeScene::DrawAttackPreviewWindow(Renderer& renderer)
{
	(void)renderer;

	ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(static_cast<float>(width) * 0.5f - 40.0f, static_cast<float>(height) - 40.0f), ImGuiCond_Always);
	if (ImGui::Begin("Attack Preview"))
	{
		ImGui::Text("Editing: %s", editingAttackDataId.c_str());
		ImGui::Text("Phase: %s", GetPreviewPhaseText());
		ImGui::Text("Current Frame: %d / %d", previewCurrentFrame, GetPreviewTotalFrames());
		ImGui::Separator();

		if (previewRenderTexture.shaderResourceView)
		{
			const ImVec2 availableSize = ImGui::GetContentRegionAvail();
			const float reservedControlHeight = 48.0f;
			const float textureAspect = static_cast<float>(PreviewTextureWidth) / static_cast<float>(PreviewTextureHeight);
			ImVec2 imageSize(
				std::max(1.0f, availableSize.x),
				std::max(120.0f, availableSize.y - reservedControlHeight));

			if (imageSize.x / imageSize.y > textureAspect)
			{
				imageSize.x = imageSize.y * textureAspect;
			}
			else
			{
				imageSize.y = imageSize.x / textureAspect;
			}

			ImGui::Image(
				static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(previewRenderTexture.shaderResourceView)),
				imageSize,
				ImVec2(0.0f, 0.0f),
				ImVec2(1.0f, 1.0f));
		}
		else
		{
			ImGui::TextWrapped("Preview RenderTexture is not available.");
		}

		if (ImGui::Button("Play", ImVec2(72.0f, 28.0f)))
		{
			if (previewCurrentFrame >= GetPreviewTotalFrames())
			{
				previewCurrentFrame = 0;
			}
			previewPlaying = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop", ImVec2(72.0f, 28.0f)))
		{
			previewPlaying = false;
		}
		ImGui::SameLine();
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
	ImGui::End();
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
		ImGui::Text("Slot: %s", editingAttackDataId.c_str());
		ImGui::InputText("Display Name", displayNameBuffer.data(), displayNameBuffer.size());

		ImGui::Separator();
		ImGui::InputInt("Damage", &draftAttack.damage);
		ImGui::InputInt("Hitstun Frames", &draftAttack.hitstunFrames);
		ImGui::InputInt("Guardstun Frames", &draftAttack.guardstunFrames);

		ImGui::Separator();
		ImGui::InputInt("Startup", &draftAttack.frame.startup);
		ImGui::InputInt("Active", &draftAttack.frame.active);
		ImGui::InputInt("Recovery", &draftAttack.frame.recovery);

		ImGui::Separator();
		int usableIndex = FindUsableStateIndex(draftAttack.usableState);
		if (ImGui::Combo("Usable State", &usableIndex, UsableStateLabels, static_cast<int>(std::size(UsableStateLabels))))
		{
			draftAttack.usableState = UsableStateValues[usableIndex];
		}

		int reactionIndex = FindHitReactionIndex(draftAttack.hitReactionType);
		if (ImGui::Combo("Hit Reaction", &reactionIndex, HitReactionLabels, static_cast<int>(std::size(HitReactionLabels))))
		{
			draftAttack.hitReactionType = HitReactionValues[reactionIndex];
		}

		if (selectedCategory == CustomizeAttackCategory::Special)
		{
			int commandIndex = FindCommandIndex(draftAttack.commandId);
			if (ImGui::Combo("Command", &commandIndex, CommandLabels, static_cast<int>(std::size(CommandLabels))))
			{
				draftAttack.commandId = CommandValues[commandIndex];
			}
		}
		else
		{
			draftAttack.commandId = AttackCommandId::None;
		}

		DrawHitboxEditor();
		ClampAttackDataValues(draftAttack);
		ClampPreviewCurrentFrame();

		ImGui::Separator();
		if (ImGui::Button("Save", ImVec2(120.0f, 30.0f)))
		{
			SaveDraftAttack();
		}
		ImGui::SameLine();
		if (ImGui::Button("Back", ImVec2(120.0f, 30.0f)))
		{
			NavigateBack();
		}

		if (!statusMessage.empty())
		{
			ImGui::TextWrapped("%s", statusMessage.c_str());
		}
	}
	ImGui::End();
}

/// <summary>
/// 現段階で編集対象にする 1 つ目の AttackBox を描画する。
/// </summary>
void CustomizeScene::DrawHitboxEditor()
{
	if (draftAttack.hitboxes.empty())
	{
		draftAttack.hitboxes.push_back(AttackHitboxData{});
	}

	AttackHitboxData& hitbox = draftAttack.hitboxes.front();
	ImGui::Separator();
	ImGui::Text("AttackBox 0");
	ImGui::DragFloat2("Offset X / Y", &hitbox.offset.x, 0.05f);
	ImGui::DragFloat2("Width / Height", &hitbox.size.x, 0.05f, 0.0f, 100.0f);
}

/// <summary>
/// 指定カテゴリとスロット番号の技データを編集対象として読み込む。
/// </summary>
/// <param name="category">編集する技カテゴリ。</param>
/// <param name="slotIndex">カテゴリ内スロット番号。</param>
void CustomizeScene::SelectAttackSlot(CustomizeAttackCategory category, int slotIndex)
{
	selectedCategory = category;
	selectedSlotIndex = slotIndex;
	editingAttackDataId = BuildAttackDataId(category, slotIndex);

	const bool hasSavedData = std::filesystem::exists(BuildAttackDataPath(editingAttackDataId));
	if (!hasSavedData || !CharacterDataLoader::LoadAttackData(editingAttackDataId, draftAttack))
	{
		draftAttack = CreateDefaultAttackData(category, slotIndex, editingAttackDataId);
		statusMessage = "New draft created.";
	}
	else
	{
		draftAttack.attackDataId = editingAttackDataId;
		statusMessage = "Loaded existing attack data.";
	}

	if (draftAttack.hitboxes.empty())
	{
		draftAttack.hitboxes.push_back(AttackHitboxData{});
	}

	CopyDisplayNameToBuffer();
	previewCurrentFrame = 0;
	previewPlaying = false;
	mode = CustomizeMode::AttackEditor;
}

/// <summary>
/// 編集中の draft を JSON として保存する。
/// </summary>
void CustomizeScene::SaveDraftAttack()
{
	SyncDraftFromEditor();
	if (AttackDataSaver::SaveAttackData(editingAttackDataId, draftAttack))
	{
		statusMessage = "Saved: assets/AttackData/" + editingAttackDataId + ".json";
		return;
	}

	statusMessage = "Save failed.";
}

/// <summary>
/// ImGui 入力欄の内容を draftAttack に反映する。
/// </summary>
void CustomizeScene::SyncDraftFromEditor()
{
	draftAttack.attackDataId = editingAttackDataId;
	draftAttack.displayName = displayNameBuffer.data();
	draftAttack.attackKind = selectedCategory == CustomizeAttackCategory::Special
		? AttackKind::Special
		: AttackKind::Normal;
	if (selectedCategory != CustomizeAttackCategory::Special)
	{
		draftAttack.commandId = AttackCommandId::None;
	}

	ClampAttackDataValues(draftAttack);
}

/// <summary>
/// 技調整プレビュー用のモデル、カメラ、RenderTexture を初期化する。
/// </summary>
void CustomizeScene::InitializePreview()
{
	ReleasePreview();

	ModelResourceManager::LoadModel(
		PreviewModelKey,
		PreviewModelPath,
		Renderer::GetDevice());

	TransformSystem::SetLocalPosition(previewPlayerTransform, Vector3(0.0f, 0.0f, 8.0f));
	TransformSystem::SetLocalEulerRotationDegrees(previewPlayerTransform, Vector3(0.0f, -90.0f, 0.0f));
	TransformSystem::SetLocalScale(previewPlayerTransform, Vector3(0.05f, 0.05f, 0.05f));
	TransformSystem::UpdateWorldTransform(previewPlayerTransform);

	TransformSystem::SetLocalPosition(previewCameraTransform, Vector3(0.0f, 4.0f, -12.0f));
	TransformSystem::SetLocalEulerRotationDegrees(previewCameraTransform, Vector3(0.0f, 0.0f, 0.0f));
	TransformSystem::SetLocalScale(previewCameraTransform, Vector3::One);
	TransformSystem::UpdateWorldTransform(previewCameraTransform);

	const float aspectRatio = static_cast<float>(PreviewTextureWidth) / static_cast<float>(PreviewTextureHeight);
	CameraSystem::SetPerspective(previewCamera, 45.0f, aspectRatio, 0.1f, 1000.0f);
	CameraSystem::Update(previewCamera, previewCameraTransform);

	const HRESULT hr = Renderer::CreateRenderTexture(previewRenderTexture, PreviewTextureWidth, PreviewTextureHeight);
	if (FAILED(hr))
	{
		DebugLog("[CustomizeScene] Preview RenderTexture creation failed. hr=", static_cast<long>(hr));
	}
}

/// <summary>
/// 技調整プレビュー用の RenderTexture を解放する。
/// </summary>
void CustomizeScene::ReleasePreview()
{
	Renderer::ReleaseRenderTexture(previewRenderTexture);
}

/// <summary>
/// Play 中なら表示フレームを 1 つ進め、技プレビュー終端に到達したら停止する。
/// </summary>
void CustomizeScene::UpdatePreviewPlayback()
{
	if (mode != CustomizeMode::AttackEditor || !previewPlaying)
	{
		return;
	}

	++previewCurrentFrame;
	if (previewCurrentFrame >= GetPreviewTotalFrames())
	{
		previewCurrentFrame = GetPreviewTotalFrames();
		previewPlaying = false;
	}
}

/// <summary>
/// プレビュー用カメラでモデルと現在フレームの AttackBox を RenderTexture へ描画する。
/// </summary>
/// <param name="renderer">描画に使う Renderer。</param>
void CustomizeScene::RenderAttackPreview(Renderer& renderer)
{
	if (!previewRenderTexture.renderTargetView)
	{
		return;
	}

	const float clearColor[4] = { 0.04f, 0.045f, 0.06f, 1.0f };
	Renderer::BeginRenderTexture(previewRenderTexture, clearColor);

	TransformSystem::UpdateWorldTransform(previewPlayerTransform);
	TransformSystem::UpdateWorldTransform(previewCameraTransform);
	CameraSystem::Update(previewCamera, previewCameraTransform);
	renderer.SetViewProjection(previewCamera.viewMatrix, previewCamera.projectionMatrix);

	const ModelResource* previewModel = ModelResourceManager::GetModel(PreviewModelKey);
	const bool drewModel = previewModel
		&& renderer.DrawModel(*previewModel, TransformSystem::GetWorldMatrix(previewPlayerTransform));
	if (!drewModel)
	{
		const Matrix fallbackWorld =
			Matrix::CreateScale(1.0f, 4.0f, 1.0f)
			* Matrix::CreateTranslation(Vector3(0.0f, 3.0f, 8.0f));
		renderer.DrawDebugCube(fallbackWorld);
	}

	DrawPreviewAttackBoxes(renderer);
	Renderer::RestoreBackBuffer();
}

/// <summary>
/// 現在フレームが active 範囲に入っている場合だけ、技の AttackBox を赤い半透明矩形で描画する。
/// </summary>
/// <param name="renderer">DebugBox 描画に使う Renderer。</param>
void CustomizeScene::DrawPreviewAttackBoxes(Renderer& renderer)
{
	if (!IsPreviewAttackActive())
	{
		return;
	}

	const Color attackBoxColor(1.0f, 0.0f, 0.0f, 0.35f);
	const Vector3 basePosition = TransformSystem::GetWorldPosition(previewPlayerTransform);

	for (const AttackHitboxData& hitbox : draftAttack.hitboxes)
	{
		if (hitbox.size.x <= 0.0f || hitbox.size.y <= 0.0f)
		{
			continue;
		}

		const Vector3 center(
			basePosition.x + hitbox.offset.x,
			basePosition.y + hitbox.offset.y,
			basePosition.z);
		const Matrix boxWorld =
			Matrix::CreateScale(hitbox.size.x * 0.5f, hitbox.size.y * 0.5f, PreviewBoxDepth * 0.5f)
			* Matrix::CreateTranslation(center);

		renderer.DrawDebugBox(boxWorld, attackBoxColor);
	}
}

/// <summary>
/// 現在フレームを、0F の Idle を含むプレビュー表示範囲内へ収める。
/// </summary>
void CustomizeScene::ClampPreviewCurrentFrame()
{
	previewCurrentFrame = std::clamp(previewCurrentFrame, 0, GetPreviewTotalFrames());
}

/// <summary>
/// プレビュー再生を止め、指定フレーム数だけ手動で進める。
/// </summary>
/// <param name="frameDelta">進めるフレーム数。負数なら戻す。</param>
void CustomizeScene::StepPreviewFrame(int frameDelta)
{
	previewPlaying = false;
	previewCurrentFrame += frameDelta;
	ClampPreviewCurrentFrame();
}

/// <summary>
/// プレビュー表示フレームを内部 actionFrame に変換し、AttackData::frame の active 範囲内か確認する。
/// </summary>
/// <returns>AttackBox を表示するフレームなら true。</returns>
bool CustomizeScene::IsPreviewAttackActive() const
{
	const int actionFrame = GetPreviewActionFrame();
	const int activeStartFrame = std::max(0, draftAttack.frame.startup);
	const int activeFrameCount = std::max(0, draftAttack.frame.active);
	return activeFrameCount > 0
		&& actionFrame >= activeStartFrame
		&& actionFrame < activeStartFrame + activeFrameCount;
}

/// <summary>
/// startup + active + recovery から、内部処理上の攻撃総フレーム数を取得する。
/// </summary>
/// <returns>最低 1F を保証した総フレーム数。プレビュー表示では 0F Idle を含めて 0..この値まで表示する。</returns>
int CustomizeScene::GetPreviewTotalFrames() const
{
	const int startup = std::max(0, draftAttack.frame.startup);
	const int active = std::max(0, draftAttack.frame.active);
	const int recovery = std::max(0, draftAttack.frame.recovery);
	return std::max(1, startup + active + recovery);
}

/// <summary>
/// プレビュー表示フレームを、内部の PlayerActionState::actionFrame 相当へ変換する。
/// </summary>
/// <returns>0F Idle は -1、1F 以降は 0 始まりの内部 actionFrame。</returns>
int CustomizeScene::GetPreviewActionFrame() const
{
	return previewCurrentFrame - 1;
}

/// <summary>
/// 現在のプレビュー表示フレームが Idle、前隙、攻撃判定中、後隙のどこにいるかを返す。
/// </summary>
/// <returns>現在フェーズの表示名。</returns>
const char* CustomizeScene::GetPreviewPhaseText() const
{
	if (previewCurrentFrame <= 0)
	{
		return "Idle";
	}

	const int actionFrame = GetPreviewActionFrame();
	const int startup = std::max(0, draftAttack.frame.startup);
	const int active = std::max(0, draftAttack.frame.active);
	const int totalFrames = GetPreviewTotalFrames();

	if (actionFrame < startup)
	{
		return "Startup";
	}
	if (active > 0 && actionFrame < startup + active)
	{
		return "Active";
	}
	if (actionFrame < totalFrames)
	{
		return "Recovery";
	}

	return "End";
}

/// <summary>
/// 技カテゴリごとのスロット数を取得する。
/// </summary>
/// <param name="category">確認する技カテゴリ。</param>
/// <returns>対象カテゴリのスロット数。</returns>
int CustomizeScene::GetAttackSlotCount(CustomizeAttackCategory category) const
{
	switch (category)
	{
	case CustomizeAttackCategory::Air:
		return AirAttackSlotCount;
	case CustomizeAttackCategory::Special:
		return SpecialAttackSlotCount;
	case CustomizeAttackCategory::Ground:
	default:
		return GroundAttackSlotCount;
	}
}

/// <summary>
/// カテゴリとスロット番号から、assets/AttackData 配下の保存 ID を作る。
/// </summary>
/// <param name="category">保存カテゴリ。</param>
/// <param name="slotIndex">カテゴリ内スロット番号。</param>
/// <returns>拡張子なしの AttackData ID。</returns>
std::string CustomizeScene::BuildAttackDataId(CustomizeAttackCategory category, int slotIndex) const
{
	std::ostringstream stream;
	stream << CategoryLabels[ToCategoryIndex(category)] << "/slot_";
	stream << std::setw(2) << std::setfill('0') << slotIndex;
	return stream.str();
}

/// <summary>
/// 未保存スロットを開いた時に使う初期 AttackData を作る。
/// </summary>
/// <param name="category">作成する技カテゴリ。</param>
/// <param name="slotIndex">カテゴリ内スロット番号。</param>
/// <param name="attackDataId">保存先 AttackData ID。</param>
/// <returns>編集開始用の初期 AttackData。</returns>
AttackData CustomizeScene::CreateDefaultAttackData(
	CustomizeAttackCategory category,
	int slotIndex,
	const std::string& attackDataId) const
{
	AttackData attackData;
	attackData.attackDataId = attackDataId;
	attackData.displayName = std::string(CategoryLabels[ToCategoryIndex(category)]) + " Slot " + std::to_string(slotIndex);
	attackData.attackKind = category == CustomizeAttackCategory::Special ? AttackKind::Special : AttackKind::Normal;
	attackData.commandId = category == CustomizeAttackCategory::Special ? AttackCommandId::Hadouken : AttackCommandId::None;
	attackData.usableState = category == CustomizeAttackCategory::Air ? AttackUsableState::Air : AttackUsableState::Ground;
	attackData.damage = 100;
	attackData.hitstunFrames = 30;
	attackData.guardstunFrames = 30;
	attackData.hitReactionType = HitReactionType::Normal;
	attackData.frame.startup = 5;
	attackData.frame.active = 3;
	attackData.frame.recovery = 10;

	AttackHitboxData hitbox;
	hitbox.offset = Vector2(2.5f, 4.0f);
	hitbox.size = Vector2(2.0f, 2.0f);
	attackData.hitboxes.push_back(hitbox);
	return attackData;
}

/// <summary>
/// draftAttack の表示名を ImGui 入力用固定バッファへコピーする。
/// </summary>
void CustomizeScene::CopyDisplayNameToBuffer()
{
	displayNameBuffer.fill('\0');
	std::snprintf(displayNameBuffer.data(), displayNameBuffer.size(), "%s", draftAttack.displayName.c_str());
}
