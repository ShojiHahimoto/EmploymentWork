#include "Controller/CustomizeAttackEditorController.h"

#include "Data/AttackDataSaver.h"
#include "Data/CharacterDataLoader.h"
#include "Data/MotionDataLoader.h"
#include "Data/MotionDataSaver.h"
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
	/// 編集値が負数や総フレーム外にならないよう補正する。
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
	RefreshFrameAdvantageInputs();

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

CustomizeAttackEditorEditState CustomizeAttackEditorController::GetEditState() const
{
	return CustomizeAttackEditorEditState{
		draft,
		hitFrameAdvantage,
		guardFrameAdvantage
	};
}

void CustomizeAttackEditorController::ApplyEditState(const CustomizeAttackEditorEditState& editState)
{
	draft = editState.attackData;
	hitFrameAdvantage = editState.hitFrameAdvantage;
	guardFrameAdvantage = editState.guardFrameAdvantage;
	draft.attackDataId = editingAttackDataId;
	ApplyFrameAdvantageInputsToDraft();
	NormalizeDraftForSelectedCategory();
	CopyDisplayNameToBuffer();
	CopyMotionDataIdToBuffer();
}

CustomizeAttackEditorAction CustomizeAttackEditorController::RequestOpenMotionEditor()
{
	EnsureDraftMotionDataId();
	return CustomizeAttackEditorAction::OpenMotionEditor;
}

CustomizeAttackEditorAction CustomizeAttackEditorController::RequestBack() const
{
	return CustomizeAttackEditorAction::Back;
}

/// <summary>編集バッファを下書きへ確定し、カテゴリ条件を整える。</summary>
void CustomizeAttackEditorController::SyncDraftFromEditor()
{
	draft.attackDataId = editingAttackDataId;
	draft.displayName = displayNameBuffer.data();
	draft.motionDataId = motionDataIdBuffer.data();
	EnsureDraftMotionDataId();
	NormalizeDraftForSelectedCategory();
}

void CustomizeAttackEditorController::NormalizeDraftForSelectedCategory()
{
	ApplyFrameAdvantageInputsToDraft();
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

void CustomizeAttackEditorController::RefreshFrameAdvantageInputs()
{
	hitFrameAdvantage = ConvertStunFramesToFrameAdvantage(draft.frame, draft.hitstunFrames);
	guardFrameAdvantage = ConvertStunFramesToFrameAdvantage(draft.frame, draft.guardstunFrames);
}

void CustomizeAttackEditorController::ApplyFrameAdvantageInputsToDraft()
{
	draft.hitstunFrames = ConvertFrameAdvantageToStunFrames(draft.frame, hitFrameAdvantage);
	draft.guardstunFrames = ConvertFrameAdvantageToStunFrames(draft.frame, guardFrameAdvantage);
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
	RefreshFrameAdvantageInputs();
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
