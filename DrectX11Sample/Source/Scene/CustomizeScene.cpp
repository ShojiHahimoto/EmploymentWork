#include "Scene/CustomizeScene.h"

#include "Data/AttackDataSaver.h"
#include "Data/CharacterDataLoader.h"
#include "Data/CharacterDataSaver.h"
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
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iterator>
#include <memory>
#include <sstream>
#include <vector>

using namespace DirectX::SimpleMath;

namespace
{
	constexpr const char* AttackDataRootPath = "assets/AttackData";
	constexpr const char* CharacterDataRootPath = "assets/CharacterData";
	constexpr const char* PreviewModelKey = "CustomizePreviewPlayer";
	constexpr const char* PreviewModelPath = "assets/model/DebugPlayer/man.fbx";
	constexpr float PreviewBoxDepth = 0.08f;
	constexpr const char* CategoryLabels[] = { "Ground", "Air", "Special" };
	constexpr const char* CharacterSlotGroupLabels[] = { "Ground", "Air", "Special" };
	constexpr AttackButtonId AttackButtonValues[] = {
		AttackButtonId::AttackA,
		AttackButtonId::AttackB,
		AttackButtonId::AttackX,
		AttackButtonId::AttackY
	};
	constexpr const char* AttackButtonLabels[] = { "A", "B", "X", "Y" };
	constexpr AttackUsableState UsableStateValues[] = {
		AttackUsableState::Ground,
		AttackUsableState::Air
	};
	constexpr const char* UsableStateLabels[] = { "Ground", "Air" };
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

	struct AttackPickerItem
	{
		std::string attackDataId;
		std::string displayName;
		AttackData attackData;
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
	/// CustomizeCharacterAttackSlotGroup を配列アクセス用の番号へ変換する。
	/// </summary>
	/// <param name="group">変換するキャラクター側の技スロット種別。</param>
	/// <returns>Ground=0, Air=1, Special=2。</returns>
	int ToCharacterSlotGroupIndex(CustomizeCharacterAttackSlotGroup group)
	{
		return static_cast<int>(group);
	}

	/// <summary>
	/// 攻撃ボタンを UI 用の短い表示名へ変換する。
	/// </summary>
	/// <param name="button">表示する攻撃ボタン。</param>
	/// <returns>A / B / X / Y の短縮名。</returns>
	const char* ToAttackButtonShortLabel(AttackButtonId button)
	{
		for (int index = 0; index < static_cast<int>(std::size(AttackButtonValues)); ++index)
		{
			if (AttackButtonValues[index] == button)
			{
				return AttackButtonLabels[index];
			}
		}

		return "-";
	}

	/// <summary>
	/// 攻撃候補 JSON のパスから、assets/AttackData 基準の拡張子なし ID を作る。
	/// </summary>
	/// <param name="attackPath">実際の AttackData JSON パス。</param>
	/// <returns>AttackList.json に保存する attackDataId。</returns>
	std::string BuildAttackDataIdFromPath(const std::filesystem::path& attackPath)
	{
		std::error_code errorCode;
		std::filesystem::path relativePath = std::filesystem::relative(attackPath, AttackDataRootPath, errorCode);
		if (errorCode)
		{
			relativePath = attackPath.filename();
		}

		relativePath.replace_extension();
		return relativePath.generic_string();
	}

	/// <summary>
	/// 通常技カテゴリから、固定する発動可能状態を取得する。
	/// </summary>
	/// <param name="category">現在の技カテゴリ。</param>
	/// <returns>地上カテゴリなら Ground、空中カテゴリなら Air。</returns>
	AttackUsableState GetFixedNormalUsableState(CustomizeAttackCategory category)
	{
		return category == CustomizeAttackCategory::Air
			? AttackUsableState::Air
			: AttackUsableState::Ground;
	}

	/// <summary>
	/// AttackUsableState を UI 表示用の文字列へ変換する。
	/// </summary>
	/// <param name="state">表示する発動可能状態。</param>
	/// <returns>Ground / Air の表示名。</returns>
	const char* ToUsableStateLabel(AttackUsableState state)
	{
		return state == AttackUsableState::Air ? "Air" : "Ground";
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
		attackData.frame.startup = std::max(2, attackData.frame.startup);
		attackData.frame.active = std::max(0, attackData.frame.active);
		attackData.frame.recovery = std::max(0, attackData.frame.recovery);
		if (attackData.usableState != AttackUsableState::Air)
		{
			attackData.usableState = AttackUsableState::Ground;
		}
		if (attackData.usableState == AttackUsableState::Air)
		{
			attackData.hitReactionType = HitReactionType::Normal;
		}

		for (AttackHitboxData& hitbox : attackData.hitboxes)
		{
			hitbox.size.x = std::max(0.0f, hitbox.size.x);
			hitbox.size.y = std::max(0.0f, hitbox.size.y);
		}

		const int totalFrames = GetAttackTotalFrames(attackData.frame);
		const int maxActionFrame = totalFrames - 1;
		attackData.cancelSetting.startFrame = std::max(0, attackData.cancelSetting.startFrame);
		attackData.cancelSetting.startFrame = std::min(attackData.cancelSetting.startFrame, maxActionFrame);
		attackData.cancelSetting.endFrame = std::max(attackData.cancelSetting.startFrame, attackData.cancelSetting.endFrame);
		attackData.cancelSetting.endFrame = std::min(attackData.cancelSetting.endFrame, maxActionFrame);
		attackData.cancelSetting.cancelTypes.erase(
			std::remove(attackData.cancelSetting.cancelTypes.begin(), attackData.cancelSetting.cancelTypes.end(), AttackCancelType::Unknown),
			attackData.cancelSetting.cancelTypes.end());
	}

	/// <summary>
	/// 指定したキャンセル種別がキャンセル設定に含まれているか確認する。
	/// </summary>
	/// <param name="cancelTypes">確認対象のキャンセル種別配列。</param>
	/// <param name="cancelType">探すキャンセル種別。</param>
	/// <returns>含まれている場合は true。</returns>
	bool HasCancelType(const std::vector<AttackCancelType>& cancelTypes, AttackCancelType cancelType)
	{
		return std::find(cancelTypes.begin(), cancelTypes.end(), cancelType) != cancelTypes.end();
	}

	/// <summary>
	/// ImGui のチェック状態に合わせて、キャンセル種別を追加または削除する。
	/// </summary>
	/// <param name="cancelTypes">編集するキャンセル種別配列。</param>
	/// <param name="cancelType">切り替えるキャンセル種別。</param>
	/// <param name="enabled">true なら追加、false なら削除する。</param>
	void SetCancelTypeEnabled(std::vector<AttackCancelType>& cancelTypes, AttackCancelType cancelType, bool enabled)
	{
		const auto found = std::find(cancelTypes.begin(), cancelTypes.end(), cancelType);
		if (enabled)
		{
			if (found == cancelTypes.end())
			{
				cancelTypes.push_back(cancelType);
			}
			return;
		}

		if (found != cancelTypes.end())
		{
			cancelTypes.erase(found);
		}
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
	case CustomizeMode::CharacterSlotSelect:
		DrawCharacterSlotSelect();
		break;
	case CustomizeMode::CharacterEditor:
		DrawCharacterEditor();
		break;
	case CustomizeMode::AttackPicker:
		DrawAttackPicker();
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
			RefreshCharacterSlotSummaries();
			mode = CustomizeMode::CharacterSlotSelect;
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
		ImGui::InputText("Attack Name", displayNameBuffer.data(), displayNameBuffer.size());

		ImGui::Separator();
		ImGui::InputInt("Damage", &draftAttack.damage);
		ImGui::InputInt("Hitstun Frames", &draftAttack.hitstunFrames);
		ImGui::InputInt("Guardstun Frames", &draftAttack.guardstunFrames);

		ImGui::Separator();
		ImGui::InputInt("Startup", &draftAttack.frame.startup);
		ImGui::InputInt("Active", &draftAttack.frame.active);
		ImGui::InputInt("Recovery", &draftAttack.frame.recovery);

		ImGui::Separator();
		if (selectedCategory == CustomizeAttackCategory::Special)
		{
			int usableIndex = FindUsableStateIndex(draftAttack.usableState);
			if (ImGui::Combo("Usable State", &usableIndex, UsableStateLabels, static_cast<int>(std::size(UsableStateLabels))))
			{
				draftAttack.usableState = UsableStateValues[usableIndex];
			}
		}
		else
		{
			draftAttack.usableState = GetFixedNormalUsableState(selectedCategory);
			ImGui::Text("Usable State: %s (Fixed)", ToUsableStateLabel(draftAttack.usableState));
		}

		if (draftAttack.usableState == AttackUsableState::Air)
		{
			draftAttack.hitReactionType = HitReactionType::Normal;
			ImGui::Text("Hit Reaction: Normal (Fixed for Air)");
		}
		else
		{
			int reactionIndex = FindHitReactionIndex(draftAttack.hitReactionType);
			if (ImGui::Combo("Hit Reaction", &reactionIndex, HitReactionLabels, static_cast<int>(std::size(HitReactionLabels))))
			{
				draftAttack.hitReactionType = HitReactionValues[reactionIndex];
			}
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
		DrawCancelSettingEditor();
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
/// 技に含まれる全 AttackBox の位置と大きさを編集する。
/// </summary>
void CustomizeScene::DrawHitboxEditor()
{
	ImGui::Separator();
	ImGui::Text("AttackBoxes");

	for (size_t index = 0; index < draftAttack.hitboxes.size();)
	{
		ImGui::PushID(static_cast<int>(index));
		AttackHitboxData& hitbox = draftAttack.hitboxes[index];

		ImGui::Text("AttackBox %zu", index);
		ImGui::DragFloat2("AttackBox Offset X / Y", &hitbox.offset.x, 0.05f);
		ImGui::DragFloat2("AttackBox Size Width / Height", &hitbox.size.x, 0.05f, 0.0f, 100.0f);

		bool deleted = false;
		if (draftAttack.hitboxes.size() > 1
			&& ImGui::Button("Delete AttackBox", ImVec2(150.0f, 24.0f)))
		{
			draftAttack.hitboxes.erase(draftAttack.hitboxes.begin() + static_cast<std::ptrdiff_t>(index));
			deleted = true;
		}

		ImGui::PopID();
		if (!deleted)
		{
			++index;
		}
	}

	if (ImGui::Button("Add AttackBox", ImVec2(150.0f, 26.0f)))
	{
		AttackHitboxData hitbox;
		hitbox.offset = Vector2(2.5f, 4.0f);
		hitbox.size = Vector2(2.0f, 2.0f);
		draftAttack.hitboxes.push_back(hitbox);
	}
}

/// <summary>
/// 技のキャンセル設定として、可能フレームと許可するキャンセル種別を編集する。
/// </summary>
void CustomizeScene::DrawCancelSettingEditor()
{
	ImGui::Separator();
	ImGui::Text("Cancel Setting");

	const bool wasAttackCancelEnabled = draftAttack.canAttackCancel;
	if (ImGui::Checkbox("Can Attack Cancel", &draftAttack.canAttackCancel)
		&& draftAttack.canAttackCancel
		&& !wasAttackCancelEnabled
		&& draftAttack.cancelSetting.startFrame == 0
		&& draftAttack.cancelSetting.endFrame == 0
		&& draftAttack.cancelSetting.cancelTypes.empty())
	{
		draftAttack.cancelSetting.startFrame = GetAttackActiveEndFrameExclusive(draftAttack.frame);
		draftAttack.cancelSetting.endFrame = std::max(draftAttack.cancelSetting.startFrame, GetPreviewTotalFrames() - 1);
		draftAttack.cancelSetting.cancelTypes.push_back(AttackCancelType::Special);
	}

	if (!draftAttack.canAttackCancel)
	{
		ImGui::TextDisabled("Cancel setting data is kept in this draft while hidden.");
		return;
	}

	AttackCancelSettingData& cancelSetting = draftAttack.cancelSetting;
	// UI 表示はプレビューと同じ 1 始まりにし、内部データだけ 0 始まりを維持する。
	int displayStartFrame = std::max(1, cancelSetting.startFrame + 1);
	int displayEndFrame = std::max(displayStartFrame, cancelSetting.endFrame + 1);
	if (ImGui::InputInt("Cancel Start Frame (Preview 1F)", &displayStartFrame))
	{
		displayStartFrame = std::max(1, displayStartFrame);
		cancelSetting.startFrame = displayStartFrame - 1;
		cancelSetting.endFrame = std::max(cancelSetting.startFrame, cancelSetting.endFrame);
	}
	displayStartFrame = cancelSetting.startFrame + 1;
	displayEndFrame = std::max(displayStartFrame, cancelSetting.endFrame + 1);
	if (ImGui::InputInt("Cancel End Frame (Preview 1F)", &displayEndFrame))
	{
		displayEndFrame = std::max(displayStartFrame, displayEndFrame);
		cancelSetting.endFrame = displayEndFrame - 1;
	}

	bool normalEnabled = HasCancelType(cancelSetting.cancelTypes, AttackCancelType::Normal);
	bool specialEnabled = HasCancelType(cancelSetting.cancelTypes, AttackCancelType::Special);
	bool jumpEnabled = HasCancelType(cancelSetting.cancelTypes, AttackCancelType::Jump);
	if (ImGui::Checkbox("Normal Cancel", &normalEnabled))
	{
		SetCancelTypeEnabled(cancelSetting.cancelTypes, AttackCancelType::Normal, normalEnabled);
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("Special Cancel", &specialEnabled))
	{
		SetCancelTypeEnabled(cancelSetting.cancelTypes, AttackCancelType::Special, specialEnabled);
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("Jump Cancel", &jumpEnabled))
	{
		SetCancelTypeEnabled(cancelSetting.cancelTypes, AttackCancelType::Jump, jumpEnabled);
	}
}

/// <summary>
/// キャラクター作成スロットの一覧を描画する。
/// </summary>
void CustomizeScene::DrawCharacterSlotSelect()
{
	ImGui::SetNextWindowPos(ImVec2(40.0f, 40.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(560.0f, 420.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Character Slots"))
	{
		if (ImGui::Button("Refresh Character Slots", ImVec2(180.0f, 28.0f)))
		{
			RefreshCharacterSlotSummaries();
		}
		ImGui::Separator();

		for (int slotIndex = 0; slotIndex < CharacterSlotCount; ++slotIndex)
		{
			ImGui::PushID(slotIndex);
			const std::string label = BuildCharacterSlotButtonLabel(slotIndex);
			if (ImGui::Button(label.c_str(), ImVec2(170.0f, 58.0f)))
			{
				SelectCharacterSlot(slotIndex);
			}
			ImGui::PopID();

			if ((slotIndex + 1) % 3 != 0)
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
/// キャラクター名と各攻撃ボタンの技割り当てを編集する。
/// </summary>
void CustomizeScene::DrawCharacterEditor()
{
	ImGui::SetNextWindowPos(ImVec2(40.0f, 40.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(720.0f, 620.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Character Editor"))
	{
		ImGui::Text("Character Id: %s", draftCharacterParameter.characterId.c_str());
		ImGui::InputText("Character Name", characterNameBuffer.data(), characterNameBuffer.size());

		DrawCharacterAttackSlotGroup(CustomizeCharacterAttackSlotGroup::Ground, "Ground Attack Slots");
		DrawCharacterAttackSlotGroup(CustomizeCharacterAttackSlotGroup::Air, "Air Attack Slots");
		DrawCharacterAttackSlotGroup(CustomizeCharacterAttackSlotGroup::Special, "Special Attack Slots");

		ImGui::Separator();
		if (ImGui::Button("Save Character", ImVec2(150.0f, 30.0f)))
		{
			SaveDraftCharacter();
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
/// 指定したキャラクター側スロットグループの割り当て一覧を描画する。
/// </summary>
/// <param name="group">描画するスロットグループ。</param>
/// <param name="label">ImGui に表示するグループ名。</param>
void CustomizeScene::DrawCharacterAttackSlotGroup(
	CustomizeCharacterAttackSlotGroup group,
	const char* label)
{
	ImGui::Separator();
	ImGui::Text("%s", label);

	std::array<CustomizeCharacterAttackSlotDraft, CharacterAttackButtonSlotCount>& drafts =
		GetCharacterAttackSlotDrafts(group);
	const bool isSpecial = group == CustomizeCharacterAttackSlotGroup::Special;
	for (int slotIndex = 0; slotIndex < CharacterAttackButtonSlotCount; ++slotIndex)
	{
		CustomizeCharacterAttackSlotDraft& draft = drafts[slotIndex];
		const std::string assignedName = draft.attackDataId.empty()
			? (isSpecial ? "(Empty)" : "(Required)")
			: (draft.attackDisplayName.empty() ? draft.attackDataId : draft.attackDisplayName);

		ImGui::PushID(static_cast<int>(group) * 10 + slotIndex);
		ImGui::Text("%s Slot %s: %s",
			CharacterSlotGroupLabels[ToCharacterSlotGroupIndex(group)],
			ToAttackButtonShortLabel(draft.button),
			assignedName.c_str());
		ImGui::SameLine(360.0f);
		if (ImGui::Button("Select", ImVec2(90.0f, 24.0f)))
		{
			pickingSlotGroup = group;
			pickingSlotIndex = slotIndex;
			mode = CustomizeMode::AttackPicker;
		}

		if (isSpecial)
		{
			ImGui::SameLine();
			if (ImGui::Button("Clear", ImVec2(80.0f, 24.0f)))
			{
				draft.attackDataId.clear();
				draft.attackDisplayName.clear();
			}
		}
		ImGui::PopID();
	}
}

/// <summary>
/// 現在選択中のキャラクター側スロットへ割り当てる AttackData 候補を描画する。
/// </summary>
void CustomizeScene::DrawAttackPicker()
{
	std::vector<AttackPickerItem> pickerItems;
	const std::filesystem::path rootPath(AttackDataRootPath);
	std::error_code errorCode;
	if (std::filesystem::exists(rootPath, errorCode))
	{
		for (const std::filesystem::directory_entry& entry :
			std::filesystem::recursive_directory_iterator(rootPath, errorCode))
		{
			if (errorCode)
			{
				break;
			}
			if (!entry.is_regular_file(errorCode) || entry.path().extension() != ".json")
			{
				continue;
			}

			const std::string attackDataId = BuildAttackDataIdFromPath(entry.path());
			AttackData attackData;
			if (!CharacterDataLoader::LoadAttackData(attackDataId, attackData)
				|| !IsAttackCompatibleWithCharacterSlotGroup(pickingSlotGroup, attackData))
			{
				continue;
			}

			AttackPickerItem item;
			item.attackDataId = attackDataId;
			item.displayName = attackData.displayName.empty() ? attackDataId : attackData.displayName;
			item.attackData = attackData;
			pickerItems.push_back(item);
		}
	}

	std::sort(
		pickerItems.begin(),
		pickerItems.end(),
		[](const AttackPickerItem& lhs, const AttackPickerItem& rhs)
		{
			return lhs.attackDataId < rhs.attackDataId;
		});

	ImGui::SetNextWindowPos(ImVec2(40.0f, 40.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(620.0f, 520.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Select Attack Data"))
	{
		const std::array<CustomizeCharacterAttackSlotDraft, CharacterAttackButtonSlotCount>& drafts =
			GetCharacterAttackSlotDrafts(pickingSlotGroup);
		const CustomizeCharacterAttackSlotDraft& targetSlot = drafts[pickingSlotIndex];
		ImGui::Text("Assign To: %s %s",
			CharacterSlotGroupLabels[ToCharacterSlotGroupIndex(pickingSlotGroup)],
			ToAttackButtonShortLabel(targetSlot.button));
		ImGui::Separator();

		for (const AttackPickerItem& item : pickerItems)
		{
			std::ostringstream label;
			label << item.displayName << "  [" << item.attackDataId << "]";
			if (ImGui::Selectable(label.str().c_str()))
			{
				AssignAttackToCharacterSlot(item.attackDataId, item.attackData);
				mode = CustomizeMode::CharacterEditor;
			}
		}

		if (pickerItems.empty())
		{
			ImGui::TextDisabled("No compatible AttackData found.");
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
	draftAttack.attackKind = category == CustomizeAttackCategory::Special ? AttackKind::Special : AttackKind::Normal;
	if (category == CustomizeAttackCategory::Special)
	{
		if (draftAttack.usableState != AttackUsableState::Air)
		{
			draftAttack.usableState = AttackUsableState::Ground;
		}
	}
	else
	{
		draftAttack.commandId = AttackCommandId::None;
		draftAttack.usableState = GetFixedNormalUsableState(category);
	}
	if (draftAttack.usableState == AttackUsableState::Air)
	{
		draftAttack.hitReactionType = HitReactionType::Normal;
	}
	ClampAttackDataValues(draftAttack);

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
		RefreshAttackSlotSummaries(selectedCategory);
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
	if (selectedCategory == CustomizeAttackCategory::Special)
	{
		if (draftAttack.usableState != AttackUsableState::Air)
		{
			draftAttack.usableState = AttackUsableState::Ground;
		}
	}
	else
	{
		draftAttack.commandId = AttackCommandId::None;
		draftAttack.usableState = GetFixedNormalUsableState(selectedCategory);
	}
	if (draftAttack.usableState == AttackUsableState::Air)
	{
		draftAttack.hitReactionType = HitReactionType::Normal;
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
	return IsAttackFrameActive(draftAttack.frame, actionFrame);
}

/// <summary>
/// AttackFrameData の新しい発生フレーム定義から、内部処理上の攻撃総フレーム数を取得する。
/// </summary>
/// <returns>最低 1F を保証した総フレーム数。プレビュー表示では 0F Idle を含めて 0..この値まで表示する。</returns>
int CustomizeScene::GetPreviewTotalFrames() const
{
	return GetAttackTotalFrames(draftAttack.frame);
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
/// 選択カテゴリのスロット JSON を確認し、一覧表示用の保存済み名を更新する。
/// </summary>
/// <param name="category">更新する技カテゴリ。</param>
void CustomizeScene::RefreshAttackSlotSummaries(CustomizeAttackCategory category)
{
	const int categoryIndex = ToCategoryIndex(category);
	const int slotCount = GetAttackSlotCount(category);
	std::array<CustomizeAttackSlotSummary, MaxAttackSlotCount>& summaries = attackSlotSummaries[categoryIndex];

	for (int slotIndex = 0; slotIndex < MaxAttackSlotCount; ++slotIndex)
	{
		CustomizeAttackSlotSummary& summary = summaries[slotIndex];
		summary = CustomizeAttackSlotSummary{};

		if (slotIndex >= slotCount)
		{
			continue;
		}

		const std::string attackDataId = BuildAttackDataId(category, slotIndex);
		if (!std::filesystem::exists(BuildAttackDataPath(attackDataId)))
		{
			continue;
		}

		AttackData loadedAttack;
		if (!CharacterDataLoader::LoadAttackData(attackDataId, loadedAttack))
		{
			continue;
		}

		summary.hasSavedData = true;
		summary.displayName = loadedAttack.displayName.empty()
			? attackDataId
			: loadedAttack.displayName;
	}
}

/// <summary>
/// スロット番号と保存済み技名をまとめた、ImGui Button 用ラベルを作る。
/// </summary>
/// <param name="category">表示する技カテゴリ。</param>
/// <param name="slotIndex">カテゴリ内スロット番号。</param>
/// <returns>表示名と ImGui ID を含むボタンラベル。</returns>
std::string CustomizeScene::BuildAttackSlotButtonLabel(CustomizeAttackCategory category, int slotIndex) const
{
	std::ostringstream label;
	label << "Slot " << std::setw(2) << std::setfill('0') << slotIndex;

	const CustomizeAttackSlotSummary& summary = attackSlotSummaries[ToCategoryIndex(category)][slotIndex];
	label << "\n";
	if (summary.hasSavedData)
	{
		label << summary.displayName;
	}
	else
	{
		label << "New Attack";
	}

	label << "##attack_slot_" << ToCategoryIndex(category) << "_" << slotIndex;
	return label.str();
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

/// <summary>
/// キャラクタースロット番号から CharacterData フォルダ名に使う ID を作る。
/// </summary>
/// <param name="slotIndex">キャラクタースロット番号。</param>
/// <returns>slot 0 は DebugPlayer、それ以外は CharacterSlotXX。</returns>
std::string CustomizeScene::BuildCharacterId(int slotIndex) const
{
	if (slotIndex == 0)
	{
		return "DebugPlayer";
	}

	std::ostringstream stream;
	stream << "CharacterSlot" << std::setw(2) << std::setfill('0') << slotIndex;
	return stream.str();
}

/// <summary>
/// キャラクタースロット番号から CharacterData 保存フォルダを作る。
/// </summary>
/// <param name="slotIndex">キャラクタースロット番号。</param>
/// <returns>assets/CharacterData 配下の保存フォルダパス。</returns>
std::string CustomizeScene::BuildCharacterFolderPath(int slotIndex) const
{
	return (std::filesystem::path(CharacterDataRootPath) / BuildCharacterId(slotIndex)).generic_string();
}

/// <summary>
/// キャラクタースロット一覧に表示するボタンラベルを作る。
/// </summary>
/// <param name="slotIndex">キャラクタースロット番号。</param>
/// <returns>スロット番号、保存済み名、ImGui ID を含むラベル。</returns>
std::string CustomizeScene::BuildCharacterSlotButtonLabel(int slotIndex) const
{
	std::ostringstream label;
	label << "Slot " << std::setw(2) << std::setfill('0') << slotIndex << "\n";

	const CustomizeCharacterSlotSummary& summary = characterSlotSummaries[slotIndex];
	if (summary.hasSavedData)
	{
		label << summary.characterName;
	}
	else
	{
		label << "New Character";
	}

	label << "##character_slot_" << slotIndex;
	return label.str();
}

/// <summary>
/// キャラクター側の固定ボタンスロットを初期化する。
/// </summary>
/// <param name="group">地上通常技、空中通常技、必殺技のどれか。</param>
/// <param name="slotIndex">ABXY の何番目か。</param>
/// <returns>未割り当て状態のスロット draft。</returns>
CustomizeCharacterAttackSlotDraft CustomizeScene::CreateCharacterAttackSlotDraft(
	CustomizeCharacterAttackSlotGroup group,
	int slotIndex) const
{
	const int clampedIndex = std::clamp(slotIndex, 0, CharacterAttackButtonSlotCount - 1);
	const AttackButtonId button = AttackButtonValues[clampedIndex];
	const std::string buttonText = ToAttackButtonShortLabel(button);

	CustomizeCharacterAttackSlotDraft draft;
	draft.button = button;
	switch (group)
	{
	case CustomizeCharacterAttackSlotGroup::Air:
		draft.slotId = std::string("AirAttack") + buttonText;
		draft.slotType = AttackSlotType::Normal;
		draft.slotUsableState = AttackUsableState::Air;
		break;
	case CustomizeCharacterAttackSlotGroup::Special:
		draft.slotId = std::string("Special") + buttonText;
		draft.slotType = AttackSlotType::Special;
		draft.slotUsableState = AttackUsableState::Unknown;
		break;
	case CustomizeCharacterAttackSlotGroup::Ground:
	default:
		draft.slotId = std::string("Attack") + buttonText;
		draft.slotType = AttackSlotType::Normal;
		draft.slotUsableState = AttackUsableState::Ground;
		break;
	}

	return draft;
}

/// <summary>
/// 指定グループのキャラクター側スロット draft 配列を取得する。
/// </summary>
/// <param name="group">取得するスロットグループ。</param>
/// <returns>変更可能な draft 配列。</returns>
std::array<CustomizeCharacterAttackSlotDraft, CustomizeScene::CharacterAttackButtonSlotCount>&
CustomizeScene::GetCharacterAttackSlotDrafts(CustomizeCharacterAttackSlotGroup group)
{
	switch (group)
	{
	case CustomizeCharacterAttackSlotGroup::Air:
		return airAttackSlotDrafts;
	case CustomizeCharacterAttackSlotGroup::Special:
		return specialAttackSlotDrafts;
	case CustomizeCharacterAttackSlotGroup::Ground:
	default:
		return groundAttackSlotDrafts;
	}
}

/// <summary>
/// 指定グループのキャラクター側スロット draft 配列を読み取り専用で取得する。
/// </summary>
/// <param name="group">取得するスロットグループ。</param>
/// <returns>読み取り専用の draft 配列。</returns>
const std::array<CustomizeCharacterAttackSlotDraft, CustomizeScene::CharacterAttackButtonSlotCount>&
CustomizeScene::GetCharacterAttackSlotDrafts(CustomizeCharacterAttackSlotGroup group) const
{
	switch (group)
	{
	case CustomizeCharacterAttackSlotGroup::Air:
		return airAttackSlotDrafts;
	case CustomizeCharacterAttackSlotGroup::Special:
		return specialAttackSlotDrafts;
	case CustomizeCharacterAttackSlotGroup::Ground:
	default:
		return groundAttackSlotDrafts;
	}
}

/// <summary>
/// 選択したキャラクタースロットを編集用 draft に読み込む。
/// </summary>
/// <param name="slotIndex">編集するキャラクタースロット番号。</param>
void CustomizeScene::SelectCharacterSlot(int slotIndex)
{
	selectedCharacterSlotIndex = std::clamp(slotIndex, 0, CharacterSlotCount - 1);
	editingCharacterFolderPath = BuildCharacterFolderPath(selectedCharacterSlotIndex);

	draftCharacterParameter = CharacterParameterData{};
	draftCharacterParameter.characterId = BuildCharacterId(selectedCharacterSlotIndex);
	draftCharacterParameter.characterName = selectedCharacterSlotIndex == 0
		? "デバッグプレイヤー"
		: "Character Slot " + std::to_string(selectedCharacterSlotIndex);

	for (int index = 0; index < CharacterAttackButtonSlotCount; ++index)
	{
		groundAttackSlotDrafts[index] = CreateCharacterAttackSlotDraft(CustomizeCharacterAttackSlotGroup::Ground, index);
		airAttackSlotDrafts[index] = CreateCharacterAttackSlotDraft(CustomizeCharacterAttackSlotGroup::Air, index);
		specialAttackSlotDrafts[index] = CreateCharacterAttackSlotDraft(CustomizeCharacterAttackSlotGroup::Special, index);
	}

	CharacterData loadedCharacter;
	const bool hasSavedData =
		std::filesystem::exists(std::filesystem::path(editingCharacterFolderPath) / "Parameter.json")
		|| std::filesystem::exists(std::filesystem::path(editingCharacterFolderPath) / "AttackList.json");
	const bool loadedCompletely = hasSavedData
		&& CharacterDataLoader::LoadCharacterData(editingCharacterFolderPath, loadedCharacter);
	if (hasSavedData)
	{
		draftCharacterParameter = loadedCharacter.parameter;
		draftCharacterParameter.characterId = BuildCharacterId(selectedCharacterSlotIndex);

		for (const CharacterAssignedAttackData& assignedAttack : loadedCharacter.attacks)
		{
			CustomizeCharacterAttackSlotGroup group = CustomizeCharacterAttackSlotGroup::Ground;
			if (assignedAttack.slotType == AttackSlotType::Special)
			{
				group = CustomizeCharacterAttackSlotGroup::Special;
			}
			else if (assignedAttack.slotUsableState == AttackUsableState::Air)
			{
				group = CustomizeCharacterAttackSlotGroup::Air;
			}

			std::array<CustomizeCharacterAttackSlotDraft, CharacterAttackButtonSlotCount>& drafts =
				GetCharacterAttackSlotDrafts(group);
			for (CustomizeCharacterAttackSlotDraft& draft : drafts)
			{
				if (draft.button != assignedAttack.button)
				{
					continue;
				}

				draft.attackDataId = assignedAttack.attack.attackDataId;
				draft.attackDisplayName = assignedAttack.attack.displayName.empty()
					? assignedAttack.attack.attackDataId
					: assignedAttack.attack.displayName;
				break;
			}
		}

		statusMessage = loadedCompletely
			? "Loaded existing character data."
			: "Loaded character draft with missing or invalid attack data.";
	}
	else
	{
		statusMessage = "New character draft created.";
	}

	CopyCharacterNameToBuffer();
	RefreshCharacterAttackSlotNames();
	mode = CustomizeMode::CharacterEditor;
}

/// <summary>
/// キャラクター編集バッファの内容を CharacterData JSON として保存する。
/// </summary>
void CustomizeScene::SaveDraftCharacter()
{
	draftCharacterParameter.characterId = BuildCharacterId(selectedCharacterSlotIndex);
	draftCharacterParameter.characterName = characterNameBuffer.data();
	if (draftCharacterParameter.characterName.empty())
	{
		statusMessage = "Character name is required.";
		return;
	}

	std::string missingSlotName;
	if (!AreRequiredCharacterAttackSlotsFilled(missingSlotName))
	{
		statusMessage = "Required slot is empty: " + missingSlotName;
		return;
	}

	std::vector<CharacterAttackSlotData> attackSlots = BuildCharacterAttackSlotsForSave();
	if (CharacterDataSaver::SaveCharacterData(editingCharacterFolderPath, draftCharacterParameter, attackSlots))
	{
		RefreshCharacterSlotSummaries();
		statusMessage = "Saved: " + editingCharacterFolderPath;
		return;
	}

	statusMessage = "Character save failed.";
}

/// <summary>
/// draftCharacterParameter のキャラクター名を ImGui 入力用固定バッファへコピーする。
/// </summary>
void CustomizeScene::CopyCharacterNameToBuffer()
{
	characterNameBuffer.fill('\0');
	std::snprintf(
		characterNameBuffer.data(),
		characterNameBuffer.size(),
		"%s",
		draftCharacterParameter.characterName.c_str());
}

/// <summary>
/// CharacterData フォルダを確認し、キャラクタースロット一覧表示用の名前を更新する。
/// </summary>
void CustomizeScene::RefreshCharacterSlotSummaries()
{
	for (int slotIndex = 0; slotIndex < CharacterSlotCount; ++slotIndex)
	{
		CustomizeCharacterSlotSummary& summary = characterSlotSummaries[slotIndex];
		summary = CustomizeCharacterSlotSummary{};

		const std::filesystem::path characterFolder(BuildCharacterFolderPath(slotIndex));
		const bool hasSavedData =
			std::filesystem::exists(characterFolder / "Parameter.json")
			|| std::filesystem::exists(characterFolder / "AttackList.json");
		if (!hasSavedData)
		{
			continue;
		}

		CharacterData loadedCharacter;
		CharacterDataLoader::LoadCharacterData(characterFolder.generic_string(), loadedCharacter);
		summary.hasSavedData = true;
		summary.characterName = loadedCharacter.parameter.characterName.empty()
			? BuildCharacterId(slotIndex)
			: loadedCharacter.parameter.characterName;
	}
}

/// <summary>
/// キャラクター draft 内の attackDataId から表示名を読み直す。
/// </summary>
void CustomizeScene::RefreshCharacterAttackSlotNames()
{
	for (CustomizeCharacterAttackSlotGroup group : {
		CustomizeCharacterAttackSlotGroup::Ground,
		CustomizeCharacterAttackSlotGroup::Air,
		CustomizeCharacterAttackSlotGroup::Special })
	{
		std::array<CustomizeCharacterAttackSlotDraft, CharacterAttackButtonSlotCount>& drafts =
			GetCharacterAttackSlotDrafts(group);
		for (CustomizeCharacterAttackSlotDraft& draft : drafts)
		{
			if (draft.attackDataId.empty())
			{
				draft.attackDisplayName.clear();
				continue;
			}

			AttackData attackData;
			if (CharacterDataLoader::LoadAttackData(draft.attackDataId, attackData))
			{
				draft.attackDisplayName = attackData.displayName.empty()
					? draft.attackDataId
					: attackData.displayName;
			}
			else
			{
				draft.attackDisplayName = draft.attackDataId;
			}
		}
	}
}

/// <summary>
/// AttackPicker で選んだ技を、現在選択中のキャラクター側スロットへ割り当てる。
/// </summary>
/// <param name="attackDataId">割り当てる AttackData ID。</param>
/// <param name="attackData">表示名確認用に読み込み済みの AttackData。</param>
void CustomizeScene::AssignAttackToCharacterSlot(const std::string& attackDataId, const AttackData& attackData)
{
	std::array<CustomizeCharacterAttackSlotDraft, CharacterAttackButtonSlotCount>& drafts =
		GetCharacterAttackSlotDrafts(pickingSlotGroup);
	CustomizeCharacterAttackSlotDraft& draft = drafts[pickingSlotIndex];
	draft.attackDataId = attackDataId;
	draft.attackDisplayName = attackData.displayName.empty() ? attackDataId : attackData.displayName;
}

/// <summary>
/// キャラクター draft から保存用 CharacterAttackSlotData 配列を作る。
/// </summary>
/// <returns>AttackList.json に保存する技スロット情報。</returns>
std::vector<CharacterAttackSlotData> CustomizeScene::BuildCharacterAttackSlotsForSave() const
{
	std::vector<CharacterAttackSlotData> result;
	const auto appendSlots =
		[&result](const std::array<CustomizeCharacterAttackSlotDraft, CharacterAttackButtonSlotCount>& drafts)
		{
			for (const CustomizeCharacterAttackSlotDraft& draft : drafts)
			{
				if (draft.attackDataId.empty())
				{
					continue;
				}

				CharacterAttackSlotData slot;
				slot.slotId = draft.slotId;
				slot.attackDataId = draft.attackDataId;
				slot.slotType = draft.slotType;
				slot.button = draft.button;
				slot.slotUsableState = draft.slotUsableState;
				result.push_back(slot);
			}
		};

	appendSlots(groundAttackSlotDrafts);
	appendSlots(airAttackSlotDrafts);
	appendSlots(specialAttackSlotDrafts);
	return result;
}

/// <summary>
/// 地上通常技と空中通常技の必須スロットが埋まっているか確認する。
/// </summary>
/// <param name="outMissingSlotName">未設定スロットがある場合の表示名。</param>
/// <returns>必須スロットが全て埋まっている場合は true。</returns>
bool CustomizeScene::AreRequiredCharacterAttackSlotsFilled(std::string& outMissingSlotName) const
{
	const auto checkSlots =
		[&outMissingSlotName](
			const std::array<CustomizeCharacterAttackSlotDraft, CharacterAttackButtonSlotCount>& drafts,
			const char* groupName)
		{
			for (const CustomizeCharacterAttackSlotDraft& draft : drafts)
			{
				if (!draft.attackDataId.empty())
				{
					continue;
				}

				outMissingSlotName = std::string(groupName) + " " + ToAttackButtonShortLabel(draft.button);
				return false;
			}

			return true;
		};

	return checkSlots(groundAttackSlotDrafts, "Ground")
		&& checkSlots(airAttackSlotDrafts, "Air");
}

/// <summary>
/// AttackData が選択中のキャラクター側スロットへ割り当て可能か確認する。
/// </summary>
/// <param name="group">割り当て先のキャラクター側スロットグループ。</param>
/// <param name="attackData">確認する AttackData。</param>
/// <returns>通常/必殺、地上/空中の条件を満たす場合は true。</returns>
bool CustomizeScene::IsAttackCompatibleWithCharacterSlotGroup(
	CustomizeCharacterAttackSlotGroup group,
	const AttackData& attackData) const
{
	switch (group)
	{
	case CustomizeCharacterAttackSlotGroup::Air:
		return attackData.attackKind == AttackKind::Normal
			&& attackData.usableState == AttackUsableState::Air;
	case CustomizeCharacterAttackSlotGroup::Special:
		return attackData.attackKind == AttackKind::Special
			&& (attackData.usableState == AttackUsableState::Ground
				|| attackData.usableState == AttackUsableState::Air);
	case CustomizeCharacterAttackSlotGroup::Ground:
	default:
		return attackData.attackKind == AttackKind::Normal
			&& attackData.usableState == AttackUsableState::Ground;
	}
}
