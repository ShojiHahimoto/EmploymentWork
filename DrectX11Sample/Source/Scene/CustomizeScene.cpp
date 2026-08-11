#include "Scene/CustomizeScene.h"

#include "Data/AttackDataSaver.h"
#include "Data/CharacterDataLoader.h"
#include "Input/InputSystem.h"
#include "Input/InputTypes.h"
#include "Scene/SceneManager.h"
#include "Scene/TitleScene.h"
#include "System/imgui-docking/imgui.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <iterator>
#include <memory>
#include <sstream>

using namespace DirectX::SimpleMath;

namespace
{
	constexpr const char* AttackDataRootPath = "assets/AttackData";
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
}

/// <summary>
/// カスタマイズシーンが保持する一時 World を破棄する。
/// </summary>
void CustomizeScene::Exit()
{
	world.Clear();
}

/// <summary>
/// キャンセル入力があれば、現在の編集階層から一つ戻る。
/// </summary>
void CustomizeScene::RunSystems()
{
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
	(void)renderer;

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
		DrawAttackEditor();
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
void CustomizeScene::DrawAttackEditor()
{
	ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(static_cast<float>(width) * 0.5f - 40.0f, static_cast<float>(height) - 40.0f), ImGuiCond_Always);
	if (ImGui::Begin("Attack Preview"))
	{
		ImGui::Text("Editing: %s", editingAttackDataId.c_str());
		ImGui::Separator();
		ImGui::Text("Preview will be added after save flow is stable.");
		ImGui::Text("Current Frame: 0");
		ImGui::Button("Play");
		ImGui::SameLine();
		ImGui::Button("Stop");
		ImGui::SameLine();
		ImGui::Button("< 1F");
		ImGui::SameLine();
		ImGui::Button("1F >");
	}
	ImGui::End();

	DrawAttackEditorWindow();
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
