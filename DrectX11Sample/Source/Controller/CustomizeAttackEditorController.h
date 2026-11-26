#pragma once

#include "Customize/CustomizeTypes.h"
#include "Data/AttackData.h"

#include <array>
#include <string>

enum class CustomizeAttackEditorAction
{
	None,
	OpenMotionEditor,
	OpenAttackEditor,
	Save,
	Undo,
	Redo,
	Back
};

/// <summary>
/// 編集画面が一時的に扱う技データと、硬直差入力値のスナップショット。
/// </summary>
struct AttackEditState
{
	AttackData attackData;		 // 表示側が編集する技パラメータのコピー。
	int hitFrameAdvantage = 0;	 // 実行時スタン値ではなく編集UIの硬直差入力値。
	int guardFrameAdvantage = 0; // 実行時スタン値ではなく編集UIの硬直差入力値。
};

/// <summary>
/// カスタマイズ画面の AttackData 下書き、保存、スロット概要を管理する。
/// </summary>
class CustomizeAttackEditorController
{
  public:
	static constexpr int MaxAttackSlotCount = 40;
	static constexpr int GroundAttackSlotCount = MaxAttackSlotCount;
	static constexpr int AirAttackSlotCount = MaxAttackSlotCount;
	static constexpr int SpecialAttackSlotCount = MaxAttackSlotCount;
	const CustomizeAttackSlotSummary &GetSlotSummary(CustomizeAttackCategory category, int slot) const;
	static constexpr int AttackNameBufferSize = 128;
	static constexpr int MotionDataIdBufferSize = 128;

	std::string SelectSlot(CustomizeAttackCategory category, int slotIndex);

	std::string SaveDraft();

	AttackEditState GetEditState() const;
	void ApplyEditState(const AttackEditState &editState);
	CustomizeAttackEditorAction RequestOpenMotionEditor();
	CustomizeAttackEditorAction RequestBack() const;

	void SyncDraftFromEditor();

	void EnsureDraftMotionDataId();

	void RefreshSlotSummaries(CustomizeAttackCategory category);


	std::string BuildAttackDataId(CustomizeAttackCategory category, int slotIndex) const;

	std::string BuildMotionDataId(CustomizeAttackCategory category, int slotIndex) const;

	int GetSlotCount(CustomizeAttackCategory category) const;

	void PrepareCommonMotionPreview(const std::string &motionDataId);

	AttackData &GetDraft();

	const AttackData &GetDraft() const;

	const std::string &GetEditingAttackDataId() const;

	CustomizeAttackCategory GetSelectedCategory() const;

	int GetSelectedSlotIndex() const;

	std::array<char, AttackNameBufferSize> &GetDisplayNameBuffer();

	std::array<char, MotionDataIdBufferSize> &GetMotionDataIdBuffer();

	const std::array<char, MotionDataIdBufferSize> &GetMotionDataIdBuffer() const;

  private:
	AttackData CreateDefaultAttackData(CustomizeAttackCategory category, int slotIndex,
									   const std::string &attackDataId) const;

	void CopyDisplayNameToBuffer();

	void CopyMotionDataIdToBuffer();

	void FromStunFramesToFrameAdvantage();

	void FromFrameAdvantageToStunFrames();
	void NormalizeDraftForSelectedCategory();

	AttackEditState state;
	std::array<char, AttackNameBufferSize> displayNameBuffer = {};
	std::array<char, MotionDataIdBufferSize> motionDataIdBuffer = {};
	std::string editingAttackDataId;
	CustomizeAttackCategory selectedCategory = CustomizeAttackCategory::Ground;
	int selectedSlotIndex = 0;
	std::array<std::array<CustomizeAttackSlotSummary, MaxAttackSlotCount>, 3> slotSummaries = {};
};
