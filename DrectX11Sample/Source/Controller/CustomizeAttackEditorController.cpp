#include "Controller/CustomizeAttackEditorController.h"

#include "Data/AttackDataSaver.h"
#include "Data/CharacterDataLoader.h"
#include "Data/MotionDataLoader.h"
#include "Data/MotionDataSaver.h"
#include "System/imgui-docking/imgui.h"

#include <SimpleMath.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <vector>

using namespace DirectX::SimpleMath;

namespace
{
	constexpr const char* AttackDataRootPath = "assets/AttackData";
	constexpr const char* CategoryLabels[] = { "Ground", "Air", "Special" };
	constexpr AttackUsableState UsableStateValues[] = {
		AttackUsableState::Ground,
		AttackUsableState::Air
	};
	constexpr const char* UsableStateLabels[] = { "Ground", "Air" };
	constexpr AttackHeight AttackHeightValues[] = {
		AttackHeight::High,
		AttackHeight::Mid,
		AttackHeight::Low
	};
	constexpr const char* AttackHeightLabels[] = { "High", "Mid", "Low" };
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
	/// CustomizeAttackCategory を配列アクセス用の番号へ変換する。
	/// </summary>
	/// <param name="category">変換するカテゴリ。</param>
	/// <returns>Ground=0、Air=1、Special=2 の番号。</returns>
	int ToCategoryIndex(CustomizeAttackCategory category)
	{
		return static_cast<int>(category);
	}

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
	/// MotionData ID から JSON ファイルの保存先パスを作る。
	/// </summary>
	/// <param name="motionDataId">assets/MotionData から見た拡張子なしのモーション ID。</param>
	/// <returns>読み書き対象の MotionData JSON ファイルパス。</returns>
	std::filesystem::path BuildMotionDataPath(const std::string& motionDataId)
	{
		return std::filesystem::path("assets/MotionData") / (motionDataId + ".json");
	}

	/// <summary>
	/// MotionData ID が攻撃モーション用の保存領域を指しているか確認する。
	/// </summary>
	/// <param name="motionDataId">確認する MotionData ID。</param>
	/// <returns>Attack/ から始まる場合は true。</returns>
	bool IsAttackMotionDataId(const std::string& motionDataId)
	{
		return motionDataId.rfind("Attack/", 0) == 0;
	}

	/// <summary>
	/// 通常技カテゴリから固定の発動可能状態を取得する。
	/// </summary>
	/// <param name="category">通常技カテゴリ。</param>
	/// <returns>地上技なら Ground、空中技なら Air。</returns>
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
	/// AttackHeight の現在値が Combo 配列の何番目かを取得する。
	/// </summary>
	/// <param name="value">検索する AttackHeight。</param>
	/// <returns>Combo 用 index。</returns>
	int FindAttackHeightIndex(AttackHeight value)
	{
		for (int index = 0; index < static_cast<int>(std::size(AttackHeightValues)); ++index)
		{
			if (AttackHeightValues[index] == value)
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
		if (attackData.attackHeight == AttackHeight::Unknown)
		{
			attackData.attackHeight = AttackHeight::High;
		}

		for (AttackHitboxData& hitbox : attackData.hitboxes)
		{
			hitbox.size.x = std::max(0.0f, hitbox.size.x);
			hitbox.size.y = std::max(0.0f, hitbox.size.y);
		}

		const int totalFrames = GetAttackTotalFrames(attackData.frame);
		const int maxActionFrame = totalFrames - 1;
		for (AttackMovementKeyData& movementKey : attackData.movementKeys)
		{
			movementKey.frame = std::clamp(movementKey.frame, 0, maxActionFrame);
		}
		std::sort(
			attackData.movementKeys.begin(),
			attackData.movementKeys.end(),
			[](const AttackMovementKeyData& left, const AttackMovementKeyData& right)
			{
				return left.frame < right.frame;
			});
		attackData.cancelSetting.startFrame = std::max(0, attackData.cancelSetting.startFrame);
		attackData.cancelSetting.startFrame = std::min(attackData.cancelSetting.startFrame, maxActionFrame);
		attackData.cancelSetting.endFrame = std::max(attackData.cancelSetting.startFrame, attackData.cancelSetting.endFrame);
		attackData.cancelSetting.endFrame = std::min(attackData.cancelSetting.endFrame, maxActionFrame);
		attackData.cancelSetting.cancelTypes.erase(
			std::remove(attackData.cancelSetting.cancelTypes.begin(), attackData.cancelSetting.cancelTypes.end(), AttackCancelType::Unknown),
			attackData.cancelSetting.cancelTypes.end());
	}
}

std::string CustomizeAttackEditorController::SelectSlot(CustomizeAttackCategory category, int slotIndex)
{
	selectedCategory = category;
	selectedSlotIndex = slotIndex;
	editingAttackDataId = BuildAttackDataId(category, slotIndex);

	std::string statusMessage;
	const bool hasSavedData = std::filesystem::exists(BuildAttackDataPath(editingAttackDataId));
	if (!hasSavedData || !CharacterDataLoader::LoadAttackData(editingAttackDataId, draft))
	{
		draft = CreateDefaultAttackData(category, slotIndex, editingAttackDataId);
		statusMessage = "New draft created.";
	}
	else
	{
		draft.attackDataId = editingAttackDataId;
		statusMessage = "Loaded existing attack data.";
	}

	if (draft.hitboxes.empty())
	{
		draft.hitboxes.push_back(AttackHitboxData{});
	}
	draft.attackKind = category == CustomizeAttackCategory::Special ? AttackKind::Special : AttackKind::Normal;
	if (category == CustomizeAttackCategory::Special)
	{
		if (draft.usableState != AttackUsableState::Air)
		{
			draft.usableState = AttackUsableState::Ground;
		}
	}
	else
	{
		draft.commandId = AttackCommandId::None;
		draft.usableState = GetFixedNormalUsableState(category);
	}
	if (draft.usableState == AttackUsableState::Air)
	{
		draft.hitReactionType = HitReactionType::Normal;
	}
	ClampAttackDataValues(draft);

	EnsureDraftMotionDataId();
	CopyDisplayNameToBuffer();
	CopyMotionDataIdToBuffer();
	return statusMessage;
}

std::string CustomizeAttackEditorController::SaveDraft()
{
	SyncDraftFromEditor();
	if (AttackDataSaver::SaveAttackData(editingAttackDataId, draft))
	{
		RefreshSlotSummaries(selectedCategory);
		return "Saved: assets/AttackData/" + editingAttackDataId + ".json";
	}

	return "Save failed.";
}

CustomizeAttackEditorAction CustomizeAttackEditorController::DrawEditorControls(
	int previewTotalFrames,
	std::string& statusMessage)
{
	ImGui::Text("Slot: %s", editingAttackDataId.c_str());
	ImGui::InputText("Attack Name", displayNameBuffer.data(), displayNameBuffer.size());
	ImGui::Text("MotionData ID: %s", motionDataIdBuffer.data());

	ImGui::Separator();
	ImGui::InputInt("Damage", &draft.damage);
	ImGui::InputInt("Hitstun Frames", &draft.hitstunFrames);
	ImGui::InputInt("Guardstun Frames", &draft.guardstunFrames);
	int attackHeightIndex = FindAttackHeightIndex(draft.attackHeight);
	if (ImGui::Combo("Attack Height", &attackHeightIndex, AttackHeightLabels, static_cast<int>(std::size(AttackHeightLabels))))
	{
		draft.attackHeight = AttackHeightValues[attackHeightIndex];
	}

	ImGui::Separator();
	ImGui::InputInt("Startup", &draft.frame.startup);
	ImGui::InputInt("Active", &draft.frame.active);
	ImGui::InputInt("Recovery", &draft.frame.recovery);

	ImGui::Separator();
	if (selectedCategory == CustomizeAttackCategory::Special)
	{
		int usableIndex = FindUsableStateIndex(draft.usableState);
		if (ImGui::Combo("Usable State", &usableIndex, UsableStateLabels, static_cast<int>(std::size(UsableStateLabels))))
		{
			draft.usableState = UsableStateValues[usableIndex];
		}
	}
	else
	{
		draft.usableState = GetFixedNormalUsableState(selectedCategory);
		ImGui::Text("Usable State: %s (Fixed)", ToUsableStateLabel(draft.usableState));
	}

	if (draft.usableState == AttackUsableState::Air)
	{
		draft.hitReactionType = HitReactionType::Normal;
		ImGui::Text("Hit Reaction: Normal (Fixed for Air)");
	}
	else
	{
		int reactionIndex = FindHitReactionIndex(draft.hitReactionType);
		if (ImGui::Combo("Hit Reaction", &reactionIndex, HitReactionLabels, static_cast<int>(std::size(HitReactionLabels))))
		{
			draft.hitReactionType = HitReactionValues[reactionIndex];
		}
	}

	if (selectedCategory == CustomizeAttackCategory::Special)
	{
		int commandIndex = FindCommandIndex(draft.commandId);
		if (ImGui::Combo("Command", &commandIndex, CommandLabels, static_cast<int>(std::size(CommandLabels))))
		{
			draft.commandId = CommandValues[commandIndex];
		}
	}
	else
	{
		draft.commandId = AttackCommandId::None;
	}

	DrawHitboxEditor();
	DrawCancelSettingEditor(previewTotalFrames);
	ClampAttackDataValues(draft);

	ImGui::Separator();
	if (ImGui::Button("Open Motion Editor", ImVec2(180.0f, 28.0f)))
	{
		EnsureDraftMotionDataId();
		return CustomizeAttackEditorAction::OpenMotionEditor;
	}

	ImGui::Separator();
	if (ImGui::Button("Save", ImVec2(120.0f, 30.0f)))
	{
		statusMessage = SaveDraft();
	}
	ImGui::SameLine();
	if (ImGui::Button("Back", ImVec2(120.0f, 30.0f)))
	{
		return CustomizeAttackEditorAction::Back;
	}

	if (!statusMessage.empty())
	{
		ImGui::TextWrapped("%s", statusMessage.c_str());
	}

	return CustomizeAttackEditorAction::None;
}

void CustomizeAttackEditorController::SyncDraftFromEditor()
{
	draft.attackDataId = editingAttackDataId;
	draft.displayName = displayNameBuffer.data();
	draft.motionDataId = motionDataIdBuffer.data();
	EnsureDraftMotionDataId();
	draft.attackKind = selectedCategory == CustomizeAttackCategory::Special
		? AttackKind::Special
		: AttackKind::Normal;
	if (selectedCategory == CustomizeAttackCategory::Special)
	{
		if (draft.usableState != AttackUsableState::Air)
		{
			draft.usableState = AttackUsableState::Ground;
		}
	}
	else
	{
		draft.commandId = AttackCommandId::None;
		draft.usableState = GetFixedNormalUsableState(selectedCategory);
	}
	if (draft.usableState == AttackUsableState::Air)
	{
		draft.hitReactionType = HitReactionType::Normal;
	}

	ClampAttackDataValues(draft);
}

void CustomizeAttackEditorController::EnsureDraftMotionDataId()
{
	const std::string expectedMotionDataId = BuildMotionDataId(selectedCategory, selectedSlotIndex);
	const std::string previousMotionDataId = draft.motionDataId;
	if (draft.motionDataId.empty()
		|| draft.motionDataId == "debug_right_arm_wave"
		|| draft.motionDataId == BuildAttackDataId(selectedCategory, selectedSlotIndex)
		|| !IsAttackMotionDataId(draft.motionDataId)
		|| draft.motionDataId != expectedMotionDataId)
	{
		const std::filesystem::path expectedPath = BuildMotionDataPath(expectedMotionDataId);
		if (!previousMotionDataId.empty()
			&& previousMotionDataId != expectedMotionDataId
			&& IsAttackMotionDataId(previousMotionDataId)
			&& !std::filesystem::exists(expectedPath))
		{
			MotionData copiedMotion;
			if (MotionDataLoader::LoadMotionData(previousMotionDataId, copiedMotion))
			{
				copiedMotion.motionDataId = expectedMotionDataId;
				MotionDataSaver::SaveMotionData(expectedMotionDataId, copiedMotion);
				MotionDataManager::UnloadAll();
			}
		}

		draft.motionDataId = expectedMotionDataId;
	}

	CopyMotionDataIdToBuffer();
}

void CustomizeAttackEditorController::RefreshSlotSummaries(CustomizeAttackCategory category)
{
	const int categoryIndex = ToCategoryIndex(category);
	const int slotCount = GetSlotCount(category);
	std::array<CustomizeAttackSlotSummary, MaxAttackSlotCount>& summaries = slotSummaries[categoryIndex];

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

std::string CustomizeAttackEditorController::BuildSlotButtonLabel(CustomizeAttackCategory category, int slotIndex) const
{
	std::ostringstream label;
	label << "Slot " << std::setw(2) << std::setfill('0') << slotIndex;

	const CustomizeAttackSlotSummary& summary = slotSummaries[ToCategoryIndex(category)][slotIndex];
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

std::string CustomizeAttackEditorController::BuildAttackDataId(CustomizeAttackCategory category, int slotIndex) const
{
	std::ostringstream stream;
	stream << CategoryLabels[ToCategoryIndex(category)] << "/slot_";
	stream << std::setw(2) << std::setfill('0') << slotIndex;
	return stream.str();
}

std::string CustomizeAttackEditorController::BuildMotionDataId(CustomizeAttackCategory category, int slotIndex) const
{
	std::ostringstream stream;
	stream << "Attack/" << CategoryLabels[ToCategoryIndex(category)] << "/slot_";
	stream << std::setw(2) << std::setfill('0') << slotIndex;
	return stream.str();
}

int CustomizeAttackEditorController::GetSlotCount(CustomizeAttackCategory category) const
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

void CustomizeAttackEditorController::PrepareCommonMotionPreview(const std::string& motionDataId)
{
	draft = AttackData();
	draft.frame.startup = 2;
	draft.frame.active = 1;
	draft.frame.recovery = 27;
	draft.motionDataId = motionDataId;
	editingAttackDataId.clear();
	motionDataIdBuffer.fill('\0');
	std::snprintf(motionDataIdBuffer.data(), motionDataIdBuffer.size(), "%s", motionDataId.c_str());
}

AttackData& CustomizeAttackEditorController::GetDraft()
{
	return draft;
}

const AttackData& CustomizeAttackEditorController::GetDraft() const
{
	return draft;
}

const std::string& CustomizeAttackEditorController::GetEditingAttackDataId() const
{
	return editingAttackDataId;
}

CustomizeAttackCategory CustomizeAttackEditorController::GetSelectedCategory() const
{
	return selectedCategory;
}

int CustomizeAttackEditorController::GetSelectedSlotIndex() const
{
	return selectedSlotIndex;
}

std::array<char, CustomizeAttackEditorController::AttackNameBufferSize>& CustomizeAttackEditorController::GetDisplayNameBuffer()
{
	return displayNameBuffer;
}

std::array<char, CustomizeAttackEditorController::MotionDataIdBufferSize>& CustomizeAttackEditorController::GetMotionDataIdBuffer()
{
	return motionDataIdBuffer;
}

const std::array<char, CustomizeAttackEditorController::MotionDataIdBufferSize>& CustomizeAttackEditorController::GetMotionDataIdBuffer() const
{
	return motionDataIdBuffer;
}

AttackData CustomizeAttackEditorController::CreateDefaultAttackData(
	CustomizeAttackCategory category,
	int slotIndex,
	const std::string& attackDataId) const
{
	AttackData attackData;
	attackData.attackDataId = attackDataId;
	attackData.displayName = std::string(CategoryLabels[ToCategoryIndex(category)]) + " Slot " + std::to_string(slotIndex);
	attackData.motionDataId = BuildMotionDataId(category, slotIndex);
	attackData.attackKind = category == CustomizeAttackCategory::Special ? AttackKind::Special : AttackKind::Normal;
	attackData.commandId = category == CustomizeAttackCategory::Special ? AttackCommandId::Hadouken : AttackCommandId::None;
	attackData.usableState = category == CustomizeAttackCategory::Air ? AttackUsableState::Air : AttackUsableState::Ground;
	attackData.attackHeight = AttackHeight::High;
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

void CustomizeAttackEditorController::DrawHitboxEditor()
{
	ImGui::Separator();
	ImGui::Text("AttackBoxes");

	for (size_t index = 0; index < draft.hitboxes.size();)
	{
		ImGui::PushID(static_cast<int>(index));
		AttackHitboxData& hitbox = draft.hitboxes[index];

		ImGui::Text("AttackBox %zu", index);
		ImGui::DragFloat2("AttackBox Offset X / Y", &hitbox.offset.x, 0.05f);
		ImGui::DragFloat2("AttackBox Size Width / Height", &hitbox.size.x, 0.05f, 0.0f, 100.0f);

		bool deleted = false;
		if (draft.hitboxes.size() > 1
			&& ImGui::Button("Delete AttackBox", ImVec2(150.0f, 24.0f)))
		{
			draft.hitboxes.erase(draft.hitboxes.begin() + static_cast<std::ptrdiff_t>(index));
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
		draft.hitboxes.push_back(hitbox);
	}
}

void CustomizeAttackEditorController::DrawCancelSettingEditor(int previewTotalFrames)
{
	ImGui::Separator();
	ImGui::Text("Cancel Setting");

	const bool wasAttackCancelEnabled = draft.canAttackCancel;
	if (ImGui::Checkbox("Can Attack Cancel", &draft.canAttackCancel)
		&& draft.canAttackCancel
		&& !wasAttackCancelEnabled
		&& draft.cancelSetting.startFrame == 0
		&& draft.cancelSetting.endFrame == 0
		&& draft.cancelSetting.cancelTypes.empty())
	{
		draft.cancelSetting.startFrame = GetAttackActiveEndFrameExclusive(draft.frame);
		draft.cancelSetting.endFrame = std::max(draft.cancelSetting.startFrame, previewTotalFrames - 1);
		draft.cancelSetting.cancelTypes.push_back(AttackCancelType::Special);
	}

	if (!draft.canAttackCancel)
	{
		ImGui::TextDisabled("Cancel setting data is kept in this draft while hidden.");
		return;
	}

	AttackCancelSettingData& cancelSetting = draft.cancelSetting;
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

void CustomizeAttackEditorController::CopyDisplayNameToBuffer()
{
	displayNameBuffer.fill('\0');
	std::snprintf(displayNameBuffer.data(), displayNameBuffer.size(), "%s", draft.displayName.c_str());
}

void CustomizeAttackEditorController::CopyMotionDataIdToBuffer()
{
	motionDataIdBuffer.fill('\0');
	std::snprintf(motionDataIdBuffer.data(), motionDataIdBuffer.size(), "%s", draft.motionDataId.c_str());
}
