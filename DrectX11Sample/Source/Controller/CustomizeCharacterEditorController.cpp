#include "Controller/CustomizeCharacterEditorController.h"

#include "Data/BattleSetupData.h"
#include "Data/CharacterDataLoader.h"
#include "Data/CharacterDataSaver.h"
#include "System/imgui-docking/imgui.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <vector>

namespace
{
	constexpr const char* CharacterDataRootPath = "assets/CharacterData";
	constexpr const char* AttackDataRootPath = "assets/AttackData";
	constexpr const char* CharacterSlotGroupLabels[] = { "Ground", "Air", "Special" };
	constexpr AttackButtonId AttackButtonValues[] = {
		AttackButtonId::AttackA,
		AttackButtonId::AttackB,
		AttackButtonId::AttackX,
		AttackButtonId::AttackY
	};
	constexpr const char* AttackButtonLabels[] = { "A", "B", "X", "Y" };

	struct AttackPickerItem
	{
		std::string attackDataId;
		std::string displayName;
		AttackData attackData;
	};

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
}

CustomizeMode CustomizeCharacterEditorController::DrawSlotSelect(std::string& statusMessage)
{
	CustomizeMode nextMode = CustomizeMode::CharacterSlotSelect;

	ImGui::SetNextWindowPos(ImVec2(40.0f, 40.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(560.0f, 420.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Character Slots"))
	{
		if (ImGui::Button("Refresh Character Slots", ImVec2(180.0f, 28.0f)))
		{
			RefreshSlotSummaries();
		}
		ImGui::Separator();

		for (int slotIndex = 0; slotIndex < CharacterSlotCount; ++slotIndex)
		{
			ImGui::PushID(slotIndex);
			const std::string label = BuildSlotButtonLabel(slotIndex);
			if (ImGui::Button(label.c_str(), ImVec2(170.0f, 58.0f)))
			{
				statusMessage = SelectSlot(slotIndex);
				nextMode = CustomizeMode::CharacterEditor;
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
			nextMode = CustomizeMode::MainMenu;
		}
	}
	ImGui::End();

	return nextMode;
}

CustomizeMode CustomizeCharacterEditorController::DrawEditor(std::string& statusMessage)
{
	CustomizeMode nextMode = CustomizeMode::CharacterEditor;

	ImGui::SetNextWindowPos(ImVec2(40.0f, 40.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(720.0f, 620.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Character Editor"))
	{
		ImGui::Text("Character Id: %s", draftParameter.characterId.c_str());
		ImGui::InputText("Character Name", characterNameBuffer.data(), characterNameBuffer.size());

		bool requestedAttackPicker = false;
		requestedAttackPicker = DrawAttackSlotGroup(CustomizeCharacterAttackSlotGroup::Ground, "Ground Attack Slots")
			|| requestedAttackPicker;
		requestedAttackPicker = DrawAttackSlotGroup(CustomizeCharacterAttackSlotGroup::Air, "Air Attack Slots")
			|| requestedAttackPicker;
		requestedAttackPicker = DrawAttackSlotGroup(CustomizeCharacterAttackSlotGroup::Special, "Special Attack Slots")
			|| requestedAttackPicker;
		if (requestedAttackPicker)
		{
			nextMode = CustomizeMode::AttackPicker;
		}

		ImGui::Separator();
		if (ImGui::Button("Save Character", ImVec2(150.0f, 30.0f)))
		{
			statusMessage = SaveDraft();
		}
		ImGui::SameLine();
		if (ImGui::Button("Back", ImVec2(120.0f, 30.0f)))
		{
			nextMode = CustomizeMode::CharacterSlotSelect;
		}

		if (!statusMessage.empty())
		{
			ImGui::TextWrapped("%s", statusMessage.c_str());
		}
	}
	ImGui::End();

	return nextMode;
}

CustomizeMode CustomizeCharacterEditorController::DrawAttackPicker(std::string& statusMessage)
{
	CustomizeMode nextMode = CustomizeMode::AttackPicker;
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
				|| !IsAttackCompatibleWithSlotGroup(pickingSlotGroup, attackData))
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
		const std::array<CustomizeCharacterAttackSlotDraft, AttackButtonSlotCount>& drafts =
			GetAttackSlotDrafts(pickingSlotGroup);
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
				AssignAttackToCurrentSlot(item.attackDataId, item.attackData);
				nextMode = CustomizeMode::CharacterEditor;
			}
		}

		if (pickerItems.empty())
		{
			ImGui::TextDisabled("No compatible AttackData found.");
		}

		ImGui::Separator();
		if (ImGui::Button("Back", ImVec2(120.0f, 28.0f)))
		{
			nextMode = CustomizeMode::CharacterEditor;
		}
	}
	ImGui::End();

	return nextMode;
}

std::string CustomizeCharacterEditorController::BuildCharacterId(int slotIndex) const
{
	return BattleSetup::BuildCharacterSlotId(slotIndex);
}

std::string CustomizeCharacterEditorController::BuildCharacterFolderPath(int slotIndex) const
{
	return (std::filesystem::path(CharacterDataRootPath) / BuildCharacterId(slotIndex)).generic_string();
}

std::string CustomizeCharacterEditorController::BuildSlotButtonLabel(int slotIndex) const
{
	std::ostringstream label;
	label << "Slot " << std::setw(2) << std::setfill('0') << slotIndex << "\n";

	const CustomizeCharacterSlotSummary& summary = slotSummaries[slotIndex];
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

std::string CustomizeCharacterEditorController::SelectSlot(int slotIndex)
{
	selectedSlotIndex = std::clamp(slotIndex, 0, CharacterSlotCount - 1);
	editingFolderPath = BuildCharacterFolderPath(selectedSlotIndex);

	draftParameter = CharacterParameterData{};
	draftParameter.characterId = BuildCharacterId(selectedSlotIndex);
	draftParameter.characterName = "Character Slot " + std::to_string(selectedSlotIndex);

	for (int index = 0; index < AttackButtonSlotCount; ++index)
	{
		groundAttackSlotDrafts[index] = CreateAttackSlotDraft(CustomizeCharacterAttackSlotGroup::Ground, index);
		airAttackSlotDrafts[index] = CreateAttackSlotDraft(CustomizeCharacterAttackSlotGroup::Air, index);
		specialAttackSlotDrafts[index] = CreateAttackSlotDraft(CustomizeCharacterAttackSlotGroup::Special, index);
	}

	CharacterData loadedCharacter;
	const bool hasSavedData =
		std::filesystem::exists(std::filesystem::path(editingFolderPath) / "Parameter.json")
		|| std::filesystem::exists(std::filesystem::path(editingFolderPath) / "AttackList.json");
	const bool loadedCompletely = hasSavedData
		&& CharacterDataLoader::LoadCharacterData(editingFolderPath, loadedCharacter);
	if (hasSavedData)
	{
		draftParameter = loadedCharacter.parameter;
		draftParameter.characterId = BuildCharacterId(selectedSlotIndex);

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

			std::array<CustomizeCharacterAttackSlotDraft, AttackButtonSlotCount>& drafts =
				GetAttackSlotDrafts(group);
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
	}

	CopyCharacterNameToBuffer();
	RefreshAttackSlotNames();
	if (!hasSavedData)
	{
		return "New character draft created.";
	}

	return loadedCompletely
		? "Loaded existing character data."
		: "Loaded character draft with missing or invalid attack data.";
}

std::string CustomizeCharacterEditorController::SaveDraft()
{
	draftParameter.characterId = BuildCharacterId(selectedSlotIndex);
	draftParameter.characterName = characterNameBuffer.data();
	if (draftParameter.characterName.empty())
	{
		return "Character name is required.";
	}

	std::string missingSlotName;
	if (!AreRequiredAttackSlotsFilled(missingSlotName))
	{
		return "Required slot is empty: " + missingSlotName;
	}

	std::vector<CharacterAttackSlotData> attackSlots = BuildAttackSlotsForSave();
	if (CharacterDataSaver::SaveCharacterData(editingFolderPath, draftParameter, attackSlots))
	{
		RefreshSlotSummaries();
		return "Saved: " + editingFolderPath;
	}

	return "Character save failed.";
}

void CustomizeCharacterEditorController::RefreshSlotSummaries()
{
	for (int slotIndex = 0; slotIndex < CharacterSlotCount; ++slotIndex)
	{
		CustomizeCharacterSlotSummary& summary = slotSummaries[slotIndex];
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

void CustomizeCharacterEditorController::RefreshAttackSlotNames()
{
	for (CustomizeCharacterAttackSlotGroup group : {
		CustomizeCharacterAttackSlotGroup::Ground,
		CustomizeCharacterAttackSlotGroup::Air,
		CustomizeCharacterAttackSlotGroup::Special })
	{
		std::array<CustomizeCharacterAttackSlotDraft, AttackButtonSlotCount>& drafts =
			GetAttackSlotDrafts(group);
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

void CustomizeCharacterEditorController::AssignAttackToCurrentSlot(
	const std::string& attackDataId,
	const AttackData& attackData)
{
	std::array<CustomizeCharacterAttackSlotDraft, AttackButtonSlotCount>& drafts =
		GetAttackSlotDrafts(pickingSlotGroup);
	CustomizeCharacterAttackSlotDraft& draft = drafts[pickingSlotIndex];
	draft.attackDataId = attackDataId;
	draft.attackDisplayName = attackData.displayName.empty() ? attackDataId : attackData.displayName;
}

bool CustomizeCharacterEditorController::IsAttackCompatibleWithSlotGroup(
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

std::array<CustomizeCharacterAttackSlotDraft, CustomizeCharacterEditorController::AttackButtonSlotCount>&
CustomizeCharacterEditorController::GetAttackSlotDrafts(CustomizeCharacterAttackSlotGroup group)
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

const std::array<CustomizeCharacterAttackSlotDraft, CustomizeCharacterEditorController::AttackButtonSlotCount>&
CustomizeCharacterEditorController::GetAttackSlotDrafts(CustomizeCharacterAttackSlotGroup group) const
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

const CharacterParameterData& CustomizeCharacterEditorController::GetDraftParameter() const
{
	return draftParameter;
}

std::array<char, CustomizeCharacterEditorController::CharacterNameBufferSize>&
CustomizeCharacterEditorController::GetCharacterNameBuffer()
{
	return characterNameBuffer;
}

void CustomizeCharacterEditorController::SetPickingSlotGroup(CustomizeCharacterAttackSlotGroup group)
{
	pickingSlotGroup = group;
}

void CustomizeCharacterEditorController::SetPickingSlotIndex(int slotIndex)
{
	pickingSlotIndex = std::clamp(slotIndex, 0, AttackButtonSlotCount - 1);
}

CustomizeCharacterAttackSlotGroup CustomizeCharacterEditorController::GetPickingSlotGroup() const
{
	return pickingSlotGroup;
}

int CustomizeCharacterEditorController::GetPickingSlotIndex() const
{
	return pickingSlotIndex;
}

CustomizeCharacterAttackSlotDraft CustomizeCharacterEditorController::CreateAttackSlotDraft(
	CustomizeCharacterAttackSlotGroup group,
	int slotIndex) const
{
	const int clampedIndex = std::clamp(slotIndex, 0, AttackButtonSlotCount - 1);
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

std::vector<CharacterAttackSlotData> CustomizeCharacterEditorController::BuildAttackSlotsForSave() const
{
	std::vector<CharacterAttackSlotData> result;
	const auto appendSlots =
		[&result](const std::array<CustomizeCharacterAttackSlotDraft, AttackButtonSlotCount>& drafts)
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

bool CustomizeCharacterEditorController::AreRequiredAttackSlotsFilled(std::string& outMissingSlotName) const
{
	const auto checkSlots =
		[&outMissingSlotName](
			const std::array<CustomizeCharacterAttackSlotDraft, AttackButtonSlotCount>& drafts,
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

void CustomizeCharacterEditorController::CopyCharacterNameToBuffer()
{
	characterNameBuffer.fill('\0');
	std::snprintf(
		characterNameBuffer.data(),
		characterNameBuffer.size(),
		"%s",
		draftParameter.characterName.c_str());
}

bool CustomizeCharacterEditorController::DrawAttackSlotGroup(
	CustomizeCharacterAttackSlotGroup group,
	const char* label)
{
	bool requestedAttackPicker = false;
	ImGui::Separator();
	ImGui::Text("%s", label);

	std::array<CustomizeCharacterAttackSlotDraft, AttackButtonSlotCount>& drafts =
		GetAttackSlotDrafts(group);
	const bool isSpecial = group == CustomizeCharacterAttackSlotGroup::Special;
	for (int slotIndex = 0; slotIndex < AttackButtonSlotCount; ++slotIndex)
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
			requestedAttackPicker = true;
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

	return requestedAttackPicker;
}
