#include "Controller/CustomizeAttackEditorController.h"

#include "Data/AttackDataSaver.h"
#include "Data/CharacterDataLoader.h"
#include "Data/MotionDataLoader.h"
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
constexpr const char *AttackDataRootPath = "assets/AttackData";
constexpr const char *CategoryLabels[] = {"Ground", "Air", "Special"};
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
std::filesystem::path BuildAttackDataPath(const std::string &attackDataId)
{
	return std::filesystem::path(AttackDataRootPath) / (attackDataId + ".json");
}

/// <summary>
/// MotionData ID が攻撃モーション用の保存領域を指しているか確認する。
/// </summary>
/// <param name="motionDataId">確認する MotionData ID。</param>
/// <returns>Attack/ から始まる場合は true。</returns>
bool IsAttackMotionDataId(const std::string &motionDataId)
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
	return category == CustomizeAttackCategory::Air ? AttackUsableState::Air : AttackUsableState::Ground;
}

/// <summary>
/// 編集値が負数や総フレーム外にならないよう補正する。
/// </summary>
/// <param name="attackData">補正する AttackData。</param>
void ClampAttackDataValues(AttackData &attackData)
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

	for (AttackHitboxData &hitbox : attackData.hitboxes)
	{
		hitbox.size.x = std::max(0.0f, hitbox.size.x);
		hitbox.size.y = std::max(0.0f, hitbox.size.y);
	}

	const int totalFrames = GetAttackTotalFrames(attackData.frame);
	const int maxActionFrame = totalFrames - 1;

	for (AttackMovementKeyData &movementKey : attackData.movementKeys)
	{
		movementKey.frame = std::clamp(movementKey.frame, 0, maxActionFrame);
	}

	std::sort(attackData.movementKeys.begin(), attackData.movementKeys.end(),
			  [](const AttackMovementKeyData &left, const AttackMovementKeyData &right)
			  {
				  return left.frame < right.frame;
			  });

	attackData.cancelSetting.startFrame = std::max(0, attackData.cancelSetting.startFrame);
	attackData.cancelSetting.startFrame = std::min(attackData.cancelSetting.startFrame, maxActionFrame);
	attackData.cancelSetting.endFrame =
		std::max(attackData.cancelSetting.startFrame, attackData.cancelSetting.endFrame);
	attackData.cancelSetting.endFrame = std::min(attackData.cancelSetting.endFrame, maxActionFrame);

	attackData.cancelSetting.cancelTypes.erase(std::remove(attackData.cancelSetting.cancelTypes.begin(),
														   attackData.cancelSetting.cancelTypes.end(),
														   AttackCancelType::Unknown),
											   attackData.cancelSetting.cancelTypes.end());
}
} // namespace

/// <summary>指定カテゴリとスロット番号の技データを編集対象として読み込む。</summary>
/// <param name="category">編集する技カテゴリ。</param>
/// <param name="slotIndex">カテゴリ内スロット番号。</param>
/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
std::string CustomizeAttackEditorController::SelectSlot(CustomizeAttackCategory category, int slotIndex)
{
	selectedCategory = category;
	selectedSlotIndex = slotIndex;
	editingAttackDataId = BuildAttackDataId(category, slotIndex);

	std::string statusMessage;
	const bool hasSavedData = std::filesystem::exists(BuildAttackDataPath(editingAttackDataId));

	if (!hasSavedData || !CharacterDataLoader::LoadAttackData(editingAttackDataId, state.attackData))
	{
		state.attackData = CreateDefaultAttackData(category, slotIndex, editingAttackDataId);
		statusMessage = "New draft created.";
	}
	else
	{
		state.attackData.attackDataId = editingAttackDataId;
		statusMessage = "Loaded existing attack data.";
	}

	if (state.attackData.hitboxes.empty())
	{
		state.attackData.hitboxes.push_back(AttackHitboxData{});
	}

	state.attackData.attackKind = category == CustomizeAttackCategory::Special ? AttackKind::Special : AttackKind::Normal;

	if (category == CustomizeAttackCategory::Special)
	{
		if (state.attackData.usableState != AttackUsableState::Air)
		{
			state.attackData.usableState = AttackUsableState::Ground;
		}
	}
	else
	{
		state.attackData.commandId = AttackCommandId::None;
		state.attackData.usableState = GetFixedNormalUsableState(category);
	}

	if (state.attackData.usableState == AttackUsableState::Air)
	{
		state.attackData.hitReactionType = HitReactionType::Normal;
	}

	ClampAttackDataValues(state.attackData);
	FromStunFramesToFrameAdvantage();

	CopyDisplayNameToBuffer();
	CopyMotionDataIdToBuffer();

	return statusMessage;
}

/// <summary>編集中の draft を JSON として保存する。</summary>
/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
std::string CustomizeAttackEditorController::SaveDraft()
{
	SyncDraftFromEditor();

	if (AttackDataSaver::SaveAttackData(editingAttackDataId, state.attackData))
	{

		return "Saved: assets/AttackData/" + editingAttackDataId + ".json";
	}

	return "Save failed.";
}

/// <summary>UI 表示用に、編集中データのコピーを取得する。</summary>
/// <returns>表示・編集に使う一時スナップショット。</returns>
AttackEditState CustomizeAttackEditorController::GetEditState() const
{
	return state;
}

/// <summary>編集結果を検証・正規化して下書きへ反映する。</summary>
/// <param name="editState">表示側が編集した一時データ。</param>
void CustomizeAttackEditorController::ApplyEditState(const AttackEditState &editState)
{
	state = editState;
	state.attackData.attackDataId = editingAttackDataId;
	NormalizeDraftForSelectedCategory();

	CopyDisplayNameToBuffer();
	CopyMotionDataIdToBuffer();
}

/// <summary>モーション編集画面を開く要求を作り、モーション ID を整える。</summary>
/// <returns>モーション編集への遷移要求。</returns>
CustomizeAttackEditorAction CustomizeAttackEditorController::RequestOpenMotionEditor()
{
	EnsureDraftMotionDataId();

	return CustomizeAttackEditorAction::OpenMotionEditor;
}

/// <summary>技編集画面から戻る要求を作る。</summary>
/// <returns>戻る遷移要求。</returns>
CustomizeAttackEditorAction CustomizeAttackEditorController::RequestBack() const
{
	return CustomizeAttackEditorAction::Back;
}

/// <summary>編集バッファを下書きへ確定し、カテゴリ条件を整える。</summary>
/// <remarks>編集入力バッファの内容を下書きへ確定する。</remarks>
void CustomizeAttackEditorController::SyncDraftFromEditor()
{
	state.attackData.attackDataId = editingAttackDataId;
	state.attackData.displayName = displayNameBuffer.data();
	state.attackData.motionDataId = motionDataIdBuffer.data();
	EnsureDraftMotionDataId();
	NormalizeDraftForSelectedCategory();
}

void CustomizeAttackEditorController::NormalizeDraftForSelectedCategory()
{
	FromFrameAdvantageToStunFrames();

	state.attackData.attackKind =
		selectedCategory == CustomizeAttackCategory::Special ? AttackKind::Special : AttackKind::Normal;

	if (selectedCategory == CustomizeAttackCategory::Special)
	{
		if (state.attackData.usableState != AttackUsableState::Air)
		{
			state.attackData.usableState = AttackUsableState::Ground;
		}
	}
	else
	{
		state.attackData.commandId = AttackCommandId::None;
		state.attackData.usableState = GetFixedNormalUsableState(selectedCategory);
	}

	if (state.attackData.usableState == AttackUsableState::Air)
	{
		state.attackData.hitReactionType = HitReactionType::Normal;
	}

	ClampAttackDataValues(state.attackData);
}

/// <summary>draft のスタンフレームから、編集 UI 用の硬直差入力値を更新する。</summary>
void CustomizeAttackEditorController::FromStunFramesToFrameAdvantage()
{
	state.hitFrameAdvantage = ConvertStunFramesToFrameAdvantage(state.attackData.frame, state.attackData.hitstunFrames);
	state.guardFrameAdvantage = ConvertStunFramesToFrameAdvantage(state.attackData.frame, state.attackData.guardstunFrames);
}

/// <summary>編集 UI の硬直差入力値を、実行時に使うスタンフレームへ反映する。</summary>
void CustomizeAttackEditorController::FromFrameAdvantageToStunFrames()
{
	state.attackData.hitstunFrames = ConvertFrameAdvantageToStunFrames(state.attackData.frame, state.hitFrameAdvantage);
	state.attackData.guardstunFrames = ConvertFrameAdvantageToStunFrames(state.attackData.frame, state.guardFrameAdvantage);
}

/// <summary>編集中の技に、スロット単位で一意になる MotionData ID を割り当てる。</summary>
void CustomizeAttackEditorController::EnsureDraftMotionDataId()
{
	const std::string expectedMotionDataId = BuildMotionDataId(selectedCategory, selectedSlotIndex);

	if (state.attackData.motionDataId.empty() || state.attackData.motionDataId == "debug_right_arm_wave" ||
		state.attackData.motionDataId == BuildAttackDataId(selectedCategory, selectedSlotIndex) ||
		!IsAttackMotionDataId(state.attackData.motionDataId) || state.attackData.motionDataId != expectedMotionDataId)
	{
		state.attackData.motionDataId = expectedMotionDataId;
	}

	CopyMotionDataIdToBuffer();
}

/// <summary>選択カテゴリのスロット JSON を確認し、一覧表示用の保存済み名を更新する。</summary>
/// <param name="category">更新する技カテゴリ。</param>
void CustomizeAttackEditorController::RefreshSlotSummaries(CustomizeAttackCategory category)
{
	const int categoryIndex = ToCategoryIndex(category);
	const int slotCount = GetSlotCount(category);
	std::array<CustomizeAttackSlotSummary, MaxAttackSlotCount> &summaries = slotSummaries[categoryIndex];

	for (int slotIndex = 0; slotIndex < MaxAttackSlotCount; ++slotIndex)
	{
		CustomizeAttackSlotSummary &summary = summaries[slotIndex];
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
			summary.hasSavedData = true;
			summary.displayName = attackDataId;

			continue;
		}

		summary.hasSavedData = true;
		summary.frame = loadedAttack.frame;
		summary.displayName = loadedAttack.displayName.empty() ? attackDataId : loadedAttack.displayName;
	}
}

/// <summary>指定した技スロットの一覧データを取得する。</summary>
/// <param name="category">技カテゴリ。</param>
/// <param name="slot">カテゴリ内のスロット番号。</param>
/// <returns>読み込み済みの概要。</returns>
const CustomizeAttackSlotSummary &CustomizeAttackEditorController::GetSlotSummary(
	CustomizeAttackCategory category, int slot) const
{
	return slotSummaries[ToCategoryIndex(category)][slot];
}


/// <summary>カテゴリとスロット番号から、assets/AttackData 配下の保存 ID を作る。</summary>
/// <param name="category">保存カテゴリ。</param>
/// <param name="slotIndex">カテゴリ内スロット番号。</param>
/// <returns>拡張子なしの AttackData ID。</returns>
std::string CustomizeAttackEditorController::BuildAttackDataId(CustomizeAttackCategory category, int slotIndex) const
{
	std::ostringstream stream;
	stream << CategoryLabels[ToCategoryIndex(category)] << "/slot_";
	stream << std::setw(2) << std::setfill('0') << slotIndex;

	return stream.str();
}

/// <summary>カテゴリとスロット番号から、assets/MotionData 配下の保存 ID を作る。</summary>
/// <param name="category">保存カテゴリ。</param>
/// <param name="slotIndex">カテゴリ内スロット番号。</param>
/// <returns>拡張子なしの MotionData ID。</returns>
std::string CustomizeAttackEditorController::BuildMotionDataId(CustomizeAttackCategory category, int slotIndex) const
{
	std::ostringstream stream;
	stream << "Attack/" << CategoryLabels[ToCategoryIndex(category)] << "/slot_";
	stream << std::setw(2) << std::setfill('0') << slotIndex;

	return stream.str();
}

/// <summary>技カテゴリごとのスロット数を取得する。</summary>
/// <param name="category">確認する技カテゴリ。</param>
/// <returns>対象カテゴリのスロット数。</returns>
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

/// <summary>汎用モーション編集時のプレビュー用 AttackData を用意する。</summary>
/// <param name="motionDataId">表示する汎用 MotionData ID。</param>
void CustomizeAttackEditorController::PrepareCommonMotionPreview(const std::string &motionDataId)
{
	state.attackData = AttackData();
	state.attackData.frame.startup = 2;
	state.attackData.frame.active = 1;
	state.attackData.frame.recovery = 27;
	state.attackData.motionDataId = motionDataId;

	FromStunFramesToFrameAdvantage();

	editingAttackDataId.clear();
	motionDataIdBuffer.fill('\0');
	std::snprintf(motionDataIdBuffer.data(), motionDataIdBuffer.size(), "%s", motionDataId.c_str());
}

/// <summary>AttackData 下書きを取得する。</summary>
/// <returns>編集可能な AttackData 参照。</returns>
AttackData &CustomizeAttackEditorController::GetDraft()
{
	return state.attackData;
}

/// <summary>AttackData 下書きを取得する。</summary>
/// <returns>読み取り専用の AttackData 参照。</returns>
const AttackData &CustomizeAttackEditorController::GetDraft() const
{
	return state.attackData;
}

/// <summary>編集中 AttackData ID を取得する。</summary>
/// <returns>編集中 AttackData ID。</returns>
const std::string &CustomizeAttackEditorController::GetEditingAttackDataId() const
{
	return editingAttackDataId;
}

/// <summary>編集中カテゴリを取得する。</summary>
/// <returns>編集中カテゴリ。</returns>
CustomizeAttackCategory CustomizeAttackEditorController::GetSelectedCategory() const
{
	return selectedCategory;
}

/// <summary>編集中スロット番号を取得する。</summary>
/// <returns>編集中スロット番号。</returns>
int CustomizeAttackEditorController::GetSelectedSlotIndex() const
{
	return selectedSlotIndex;
}

/// <summary>表示名入力用バッファを取得する。</summary>
/// <returns>ImGui InputText に渡す固定長バッファ。</returns>
std::array<char, CustomizeAttackEditorController::AttackNameBufferSize> &CustomizeAttackEditorController::
	GetDisplayNameBuffer()
{
	return displayNameBuffer;
}

/// <summary>MotionData ID 入力用バッファを取得する。</summary>
/// <returns>ImGui 表示に使う固定長バッファ。</returns>
std::array<char, CustomizeAttackEditorController::MotionDataIdBufferSize> &CustomizeAttackEditorController::
	GetMotionDataIdBuffer()
{
	return motionDataIdBuffer;
}

/// <summary>MotionData ID 入力用バッファを取得する。</summary>
/// <returns>読み取り専用の固定長バッファ。</returns>
const std::array<char, CustomizeAttackEditorController::MotionDataIdBufferSize> &CustomizeAttackEditorController::
	GetMotionDataIdBuffer() const
{
	return motionDataIdBuffer;
}

/// <summary>未保存スロットを開いた時に使う初期 AttackData を作る。</summary>
/// <param name="category">作成する技カテゴリ。</param>
/// <param name="slotIndex">カテゴリ内スロット番号。</param>
/// <param name="attackDataId">保存先 AttackData ID。</param>
/// <returns>編集開始用の初期 AttackData。</returns>
AttackData CustomizeAttackEditorController::CreateDefaultAttackData(CustomizeAttackCategory category, int slotIndex,
																	const std::string &attackDataId) const
{
	AttackData attackData;
	attackData.attackDataId = attackDataId;
	attackData.displayName =
		std::string(CategoryLabels[ToCategoryIndex(category)]) + " Slot " + std::to_string(slotIndex);
	attackData.motionDataId = BuildMotionDataId(category, slotIndex);
	attackData.attackKind = category == CustomizeAttackCategory::Special ? AttackKind::Special : AttackKind::Normal;
	attackData.commandId =
		category == CustomizeAttackCategory::Special ? AttackCommandId::Hadouken : AttackCommandId::None;
	attackData.usableState =
		category == CustomizeAttackCategory::Air ? AttackUsableState::Air : AttackUsableState::Ground;
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

/// <summary>draft の表示名を ImGui 入力用固定バッファへコピーする。</summary>
void CustomizeAttackEditorController::CopyDisplayNameToBuffer()
{
	displayNameBuffer.fill('\0');
	std::snprintf(displayNameBuffer.data(), displayNameBuffer.size(), "%s", state.attackData.displayName.c_str());
}

/// <summary>draft の MotionData ID を ImGui 入力用固定バッファへコピーする。</summary>
void CustomizeAttackEditorController::CopyMotionDataIdToBuffer()
{
	motionDataIdBuffer.fill('\0');
	std::snprintf(motionDataIdBuffer.data(), motionDataIdBuffer.size(), "%s", state.attackData.motionDataId.c_str());
}
