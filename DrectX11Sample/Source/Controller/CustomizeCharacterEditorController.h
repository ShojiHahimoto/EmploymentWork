#pragma once

#include "Customize/CustomizeTypes.h"
#include "Data/CharacterData.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

struct CustomizeCharacterSlotSelection
{
	std::optional<int> slotIndex;
	bool backRequested = false;
};

/// <summary>CharacterData の下書き、技の割り当て、保存と読み込みを管理する。</summary>
class CustomizeCharacterEditorController
{
  public:
	static constexpr int CharacterSlotCount = 10;
	static constexpr int AttackButtonSlotCount = 4;
	static constexpr int CharacterNameBufferSize = 128;

	CustomizeCharacterSlotSelection DrawSlotSelect();

	CustomizeMode DrawEditor(std::string &statusMessage);

	CustomizeMode DrawAttackPicker(std::string &statusMessage);

	std::string BuildCharacterId(int slotIndex) const;

	std::string BuildCharacterFolderPath(int slotIndex) const;

	std::string BuildSlotButtonLabel(int slotIndex) const;

	std::string SelectSlot(int slotIndex);

	std::string SaveDraft();

	void RefreshSlotSummaries();

	bool IsAttackCompatibleWithSlotGroup(CustomizeCharacterAttackSlotGroup group, const AttackData &attackData) const;

	std::array<CustomizeCharacterAttackSlotDraft, AttackButtonSlotCount> &GetAttackSlotDrafts(
		CustomizeCharacterAttackSlotGroup group);

	const std::array<CustomizeCharacterAttackSlotDraft, AttackButtonSlotCount> &GetAttackSlotDrafts(
		CustomizeCharacterAttackSlotGroup group) const;

	const CharacterParameterData &GetDraftParameter() const;

	std::array<char, CharacterNameBufferSize> &GetCharacterNameBuffer();

	void SetPickingSlotGroup(CustomizeCharacterAttackSlotGroup group);

	void SetPickingSlotIndex(int slotIndex);

	CustomizeCharacterAttackSlotGroup GetPickingSlotGroup() const;

	int GetPickingSlotIndex() const;

  private:
	CustomizeCharacterAttackSlotDraft CreateAttackSlotDraft(CustomizeCharacterAttackSlotGroup group,
															int slotIndex) const;

	std::vector<CharacterAttackSlotData> BuildAttackSlotsForSave() const;

	bool AreRequiredAttackSlotsFilled(std::string &outMissingSlotName) const;

	void CopyCharacterNameToBuffer();

	void RefreshAttackSlotNames();

	bool DrawAttackSlotGroup(CustomizeCharacterAttackSlotGroup group, const char *label);

	void AssignAttackToCurrentSlot(const std::string &attackDataId, const AttackData &attackData);

	CharacterParameterData draftParameter;
	std::array<char, CharacterNameBufferSize> characterNameBuffer = {};
	std::array<CustomizeCharacterSlotSummary, CharacterSlotCount> slotSummaries = {};
	std::array<CustomizeCharacterAttackSlotDraft, AttackButtonSlotCount> groundAttackSlotDrafts = {};
	std::array<CustomizeCharacterAttackSlotDraft, AttackButtonSlotCount> airAttackSlotDrafts = {};
	std::array<CustomizeCharacterAttackSlotDraft, AttackButtonSlotCount> specialAttackSlotDrafts = {};
	CustomizeCharacterAttackSlotGroup pickingSlotGroup = CustomizeCharacterAttackSlotGroup::Ground;
	int selectedSlotIndex = 0;
	int pickingSlotIndex = 0;
	std::string editingFolderPath;
};
