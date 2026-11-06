#include "UI/CustomizeAttackEditorImGuiView.h"

#include "System/imgui-docking/imgui.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <vector>

namespace
{
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

	/// <summary>発動状態を選択UIの項目番号へ変換する。</summary>
	/// <param name="value">表示する発動状態。</param>
	/// <returns>対応する項目番号。未定義値は先頭項目。</returns>
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

	/// <summary>攻撃属性を選択UIの項目番号へ変換する。</summary>
	/// <param name="value">表示する攻撃属性。</param>
	/// <returns>対応する項目番号。未定義値は先頭項目。</returns>
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

	/// <summary>ヒットリアクションを選択UIの項目番号へ変換する。</summary>
	/// <param name="value">表示するリアクション。</param>
	/// <returns>対応する項目番号。未定義値は先頭項目。</returns>
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

	/// <summary>コマンド種別を選択UIの項目番号へ変換する。</summary>
	/// <param name="value">表示するコマンド。</param>
	/// <returns>対応する項目番号。未定義値は先頭項目。</returns>
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

	/// <summary>キャンセル種別配列に指定種別が含まれているか確認する。</summary>
	/// <param name="cancelTypes">検索するキャンセル種別列。</param>
	/// <param name="cancelType">探す種別。</param>
	/// <returns>含まれている場合は true。</returns>
	bool HasCancelType(const std::vector<AttackCancelType>& cancelTypes, AttackCancelType cancelType)
	{
		return std::find(cancelTypes.begin(), cancelTypes.end(), cancelType) != cancelTypes.end();
	}

	/// <summary>チェック状態に応じてキャンセル種別を重複なく追加・削除する。</summary>
	/// <param name="cancelTypes">編集するキャンセル種別列。</param>
	/// <param name="cancelType">追加または削除する種別。</param>
	/// <param name="enabled">有効化する場合は true。</param>
	void SetCancelTypeEnabled(
		std::vector<AttackCancelType>& cancelTypes,
		AttackCancelType cancelType,
		bool enabled)
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
/// 現行の ImGui 操作を表示アダプターへ閉じ込め、技データの確定処理を Controller に渡す。
/// </summary>
/// <param name="controller">編集中データと保存処理を所有する Controller。</param>
/// <param name="previewTotalFrames">プレビュー上の総フレーム数。</param>
/// <param name="statusMessage">保存結果などの表示文字列。</param>
/// <returns>モーション編集または戻る操作の要求。</returns>
CustomizeAttackEditorAction CustomizeAttackEditorImGuiView::Draw(
	CustomizeAttackEditorController& controller,
	int previewTotalFrames,
	std::string& statusMessage)
{
	CustomizeAttackEditorEditState editState = controller.GetEditState();
	AttackData& attackData = editState.attackData;
	const CustomizeAttackCategory category = controller.GetSelectedCategory();

	ImGui::Text("Slot: %s", controller.GetEditingAttackDataId().c_str());
	std::array<char, CustomizeAttackEditorController::AttackNameBufferSize>& displayNameBuffer =
		controller.GetDisplayNameBuffer();
	ImGui::InputText("Attack Name", displayNameBuffer.data(), displayNameBuffer.size());
	attackData.displayName = displayNameBuffer.data();
	ImGui::Text("MotionData ID: %s", controller.GetMotionDataIdBuffer().data());

	ImGui::Separator();
	ImGui::InputInt("Damage", &attackData.damage);
	ImGui::InputInt("Hit Frame Advantage", &editState.hitFrameAdvantage);
	ImGui::InputInt("Guard Frame Advantage", &editState.guardFrameAdvantage);
	int attackHeightIndex = FindAttackHeightIndex(attackData.attackHeight);
	if (ImGui::Combo("Attack Height", &attackHeightIndex, AttackHeightLabels, static_cast<int>(std::size(AttackHeightLabels))))
	{
		attackData.attackHeight = AttackHeightValues[attackHeightIndex];
	}

	ImGui::Separator();
	ImGui::InputInt("Startup", &attackData.frame.startup);
	ImGui::InputInt("Active", &attackData.frame.active);
	ImGui::InputInt("Recovery", &attackData.frame.recovery);
	ImGui::TextDisabled(
		"Saved stun frames: Hit %d / Guard %d",
		ConvertFrameAdvantageToStunFrames(attackData.frame, editState.hitFrameAdvantage),
		ConvertFrameAdvantageToStunFrames(attackData.frame, editState.guardFrameAdvantage));

	ImGui::Separator();
	if (category == CustomizeAttackCategory::Special)
	{
		int usableIndex = FindUsableStateIndex(attackData.usableState);
		if (ImGui::Combo("Usable State", &usableIndex, UsableStateLabels, static_cast<int>(std::size(UsableStateLabels))))
		{
			attackData.usableState = UsableStateValues[usableIndex];
		}
	}
	else
	{
		attackData.usableState = category == CustomizeAttackCategory::Air
			? AttackUsableState::Air
			: AttackUsableState::Ground;
		ImGui::Text("Usable State: %s (Fixed)", category == CustomizeAttackCategory::Air ? "Air" : "Ground");
	}

	if (attackData.usableState == AttackUsableState::Air)
	{
		attackData.hitReactionType = HitReactionType::Normal;
		ImGui::Text("Hit Reaction: Normal (Fixed for Air)");
	}
	else
	{
		int reactionIndex = FindHitReactionIndex(attackData.hitReactionType);
		if (ImGui::Combo("Hit Reaction", &reactionIndex, HitReactionLabels, static_cast<int>(std::size(HitReactionLabels))))
		{
			attackData.hitReactionType = HitReactionValues[reactionIndex];
		}
	}

	if (category == CustomizeAttackCategory::Special)
	{
		int commandIndex = FindCommandIndex(attackData.commandId);
		if (ImGui::Combo("Command", &commandIndex, CommandLabels, static_cast<int>(std::size(CommandLabels))))
		{
			attackData.commandId = CommandValues[commandIndex];
		}
	}
	else
	{
		attackData.commandId = AttackCommandId::None;
	}

	DrawHitboxEditor(attackData);
	DrawCancelSettingEditor(attackData, previewTotalFrames);
	controller.ApplyEditState(editState);

	ImGui::Separator();
	if (ImGui::Button("Open Motion Editor", ImVec2(180.0f, 28.0f)))
	{
		return controller.RequestOpenMotionEditor();
	}

	ImGui::Separator();
	if (ImGui::Button("Save", ImVec2(120.0f, 30.0f)))
	{
		statusMessage = controller.SaveDraft();
	}
	ImGui::SameLine();
	if (ImGui::Button("Back", ImVec2(120.0f, 30.0f)))
	{
		return controller.RequestBack();
	}

	if (!statusMessage.empty())
	{
		ImGui::TextWrapped("%s", statusMessage.c_str());
	}
	return CustomizeAttackEditorAction::None;
}

/// <summary>攻撃判定の位置・大きさ・追加削除を編集する。</summary>
void CustomizeAttackEditorImGuiView::DrawHitboxEditor(AttackData& attackData)
{
	ImGui::Separator();
	ImGui::Text("AttackBoxes");

	for (size_t index = 0; index < attackData.hitboxes.size();)
	{
		ImGui::PushID(static_cast<int>(index));
		AttackHitboxData& hitbox = attackData.hitboxes[index];
		ImGui::Text("AttackBox %zu", index);
		ImGui::DragFloat2("AttackBox Offset X / Y", &hitbox.offset.x, 0.05f);
		ImGui::DragFloat2("AttackBox Size Width / Height", &hitbox.size.x, 0.05f, 0.0f, 100.0f);

		bool deleted = false;
		if (attackData.hitboxes.size() > 1 && ImGui::Button("Delete AttackBox", ImVec2(150.0f, 24.0f)))
		{
			attackData.hitboxes.erase(attackData.hitboxes.begin() + static_cast<std::ptrdiff_t>(index));
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
		hitbox.offset = DirectX::SimpleMath::Vector2(2.5f, 4.0f);
		hitbox.size = DirectX::SimpleMath::Vector2(2.0f, 2.0f);
		attackData.hitboxes.push_back(hitbox);
	}
}

/// <summary>キャンセル可能フレームと対象種別を編集する。</summary>
/// <param name="attackData">編集対象の技データ。</param>
/// <param name="previewTotalFrames">プレビュー上の総フレーム数。</param>
void CustomizeAttackEditorImGuiView::DrawCancelSettingEditor(AttackData& attackData, int previewTotalFrames)
{
	ImGui::Separator();
	ImGui::Text("Cancel Setting");

	const bool wasAttackCancelEnabled = attackData.canAttackCancel;
	if (ImGui::Checkbox("Can Attack Cancel", &attackData.canAttackCancel)
		&& attackData.canAttackCancel
		&& !wasAttackCancelEnabled
		&& attackData.cancelSetting.startFrame == 0
		&& attackData.cancelSetting.endFrame == 0
		&& attackData.cancelSetting.cancelTypes.empty())
	{
		attackData.cancelSetting.startFrame = GetAttackActiveEndFrameExclusive(attackData.frame);
		attackData.cancelSetting.endFrame = std::max(attackData.cancelSetting.startFrame, previewTotalFrames - 1);
		attackData.cancelSetting.cancelTypes.push_back(AttackCancelType::Special);
	}

	if (!attackData.canAttackCancel)
	{
		ImGui::TextDisabled("Cancel setting data is kept in this draft while hidden.");
		return;
	}

	AttackCancelSettingData& cancelSetting = attackData.cancelSetting;
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
