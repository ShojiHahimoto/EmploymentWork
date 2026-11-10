#include "Controller/CustomizeMotionEditorController.h"
#include "Data/PosePreset.h"
#include "System/imgui-docking/imgui.h"
#include <algorithm>
#include <vector>
using namespace DirectX::SimpleMath;
namespace
{
    constexpr const char* IdlePosePresetId = "__IdlePose__";
    int ClampBodyPartIndex(int index) { return std::clamp(index, 0, MotionBodyPartCount - 1); }
	/// <summary>
	/// 読み込み UI に表示する姿勢プリセット候補。
	/// </summary>
	struct PosePresetListItem
	{
		std::string presetId;
		std::string displayName;
		bool idleMotion = false;
	};

	/// <summary>
	/// 保存済みプリセットと Idle 参照プリセットを一覧化する。
	/// </summary>
	/// <returns>読み込み UI に表示するプリセット候補。</returns>
	std::vector<PosePresetListItem> BuildPosePresetList()
	{
		std::vector<PosePresetListItem> items;
		items.push_back({ IdlePosePresetId, "Idle Pose (Common/Idle)", true });

		for (const std::string& presetId : PosePresetStore::ListPresetIds())
		{
			PosePresetData preset;
			std::string displayName = presetId;
			if (PosePresetStore::LoadPreset(presetId, preset) && !preset.displayName.empty())
			{
				displayName = preset.displayName;
			}

			items.push_back({ presetId, displayName, false });
		}

		return items;
	}

}
bool CustomizeMotionEditorController::DrawEditor(
	AttackData& attackData,
	CustomizePreviewController& previewController,
	bool editingCommonMotion,
	int totalFrames,
	std::string& statusMessage,
	Vector2& attackMovementKeyOffset)
{
	ImGui::Separator();
	if (!hasDraft)
	{
		ImGui::TextDisabled("No MotionData loaded.");
		return false;
	}

	ImGui::InputText("Motion Name", displayNameBuffer.data(), displayNameBuffer.size());
	if (editingCommonMotion)
	{
		ImGui::InputInt("Motion Total Frames", &draft.totalFrames);
		draft.totalFrames = std::max(1, draft.totalFrames);
		ImGui::Checkbox("Motion Looping", &draft.looping);
		previewController.ClampCurrentFrame(totalFrames);
	}
	else
	{
		draft.totalFrames = totalFrames;
		ImGui::Text("Motion Total Frames: %d (AttackData)", draft.totalFrames);
		draft.looping = false;
		ImGui::Text("Motion Looping: false (Attack Motion)");
	}

	const int actionFrame = previewController.GetActionFrame();
	bool canEditCurrentFrame = HasMotionKeyframe(actionFrame);
	const bool hasMovementKey = !editingCommonMotion
		&& actionFrame >= 0
		&& HasAttackMovementKeyframe(attackData, actionFrame);
	const bool hasRootOffsetKey = editingCommonMotion && HasRootOffsetKeyframe(actionFrame);
	const int pickedBodyPartIndex = previewController.ConsumePickedBodyPartIndex();
	if (pickedBodyPartIndex == -2)
	{
		SelectBodyPart(previewController, -1, actionFrame);
	}
	if (pickedBodyPartIndex >= 0 && pickedBodyPartIndex < MotionEditorBoneCount)
	{
		SelectBodyPart(previewController, pickedBodyPartIndex, actionFrame);
		statusMessage = std::string("Selected part: ") + MotionSkeleton::GetBodyPartName(selectedBoneIndex);
	}
	ImGui::Text("Selected Key Frame: %d", std::max(0, actionFrame));

	if (ImGui::Button("Add Whole Body Keyframe", ImVec2(210.0f, 28.0f)))
	{
		statusMessage = AddWholeBodyKeyframe(actionFrame, totalFrames);
		canEditCurrentFrame = HasMotionKeyframe(actionFrame);
	}
	ImGui::SameLine();
	if (ImGui::Button("Delete Current Keyframe", ImVec2(190.0f, 28.0f)))
	{
		statusMessage = DeleteWholeBodyKeyframe(actionFrame);
		canEditCurrentFrame = HasMotionKeyframe(actionFrame);
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(!canEditCurrentFrame);
	if (ImGui::Button("Copy Pose", ImVec2(100.0f, 28.0f)))
	{
		statusMessage = CopyWholeBodyPose(actionFrame);
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(!hasCopiedPose);
	if (ImGui::Button("Paste Pose", ImVec2(100.0f, 28.0f)))
	{
		statusMessage = PasteWholeBodyPose(actionFrame);
	}
	ImGui::EndDisabled();
	ImGui::EndDisabled();

	DrawPosePresetControls(actionFrame, totalFrames, statusMessage);

	ImGui::Separator();
	ImGui::Text("Pose Edit");
	if (!canEditCurrentFrame)
	{
		ImGui::TextDisabled("Add a keyframe to this frame before editing the pose.");
	}

	ImGui::BeginDisabled(!canEditCurrentFrame);
	const int previousBoneIndex = selectedBoneIndex;
	bool partChanged = false;
	if (ImGui::BeginCombo("Target Part", selectedBoneIndex >= 0 ? MotionSkeleton::GetBodyPartName(selectedBoneIndex) : "None"))
	{
		for (int bodyPartIndex = 0; bodyPartIndex < MotionEditorBoneCount; ++bodyPartIndex)
		{
			const bool isSelected = selectedBoneIndex == bodyPartIndex;
			if (ImGui::Selectable(MotionSkeleton::GetBodyPartName(bodyPartIndex), isSelected))
			{
				SelectBodyPart(previewController, bodyPartIndex, actionFrame);
				partChanged = true;
			}
			if (isSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	if (partChanged && previousBoneIndex != selectedBoneIndex && canEditCurrentFrame)
	{
		RefreshFrameEditValues(actionFrame);
	}
	ImGui::BeginDisabled(selectedBoneIndex < 0);
	if (ImGui::DragFloat3("Rotation Euler Degrees X / Y / Z", &rotationEulerDegrees.x, 0.5f))
	{
		statusMessage = SetSelectedRotationKey(previewController, actionFrame, totalFrames);
	}
	ImGui::EndDisabled();
	ImGui::EndDisabled();

	DrawIkControls(previewController, actionFrame, statusMessage);

	if (!editingCommonMotion)
	{
		ImGui::Separator();
		ImGui::Text("Attack Movement Edit");
		if (actionFrame < 0)
		{
			ImGui::TextDisabled("Select preview frame 1 or later before editing attack movement.");
		}

		ImGui::BeginDisabled(actionFrame < 0);
		if (ImGui::Button("Add Movement Keyframe", ImVec2(190.0f, 28.0f)))
		{
			AddAttackMovementKeyframe(attackData, actionFrame, attackMovementKeyOffset);
			statusMessage = "Added attack movement keyframe.";
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(!hasMovementKey);
		if (ImGui::Button("Delete Movement Keyframe", ImVec2(210.0f, 28.0f)))
		{
			DeleteAttackMovementKeyframe(attackData, actionFrame, attackMovementKeyOffset);
			statusMessage = "Deleted attack movement keyframe.";
		}
		ImGui::EndDisabled();

		ImGui::BeginDisabled(!hasMovementKey);
		if (ImGui::DragFloat2("Attack Movement Offset Forward / Up", &attackMovementKeyOffset.x, 0.05f))
		{
			UpdateAttackMovementKeyframe(attackData, actionFrame, attackMovementKeyOffset);
			statusMessage = "Set attack movement keyframe.";
		}
		ImGui::EndDisabled();
		ImGui::EndDisabled();
	}
	else
	{
		ImGui::Separator();
		ImGui::Text("Common Motion Visual Offset Edit");
		if (actionFrame < 0)
		{
			ImGui::TextDisabled("Select preview frame 1 or later before editing common motion offset.");
		}

		ImGui::BeginDisabled(actionFrame < 0);
		if (ImGui::Button("Add Visual Offset Keyframe", ImVec2(220.0f, 28.0f)))
		{
			statusMessage = AddRootOffsetKeyframe(actionFrame);
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(!hasRootOffsetKey);
		if (ImGui::Button("Delete Visual Offset Keyframe", ImVec2(230.0f, 28.0f)))
		{
			statusMessage = DeleteRootOffsetKeyframe(actionFrame);
		}
		ImGui::EndDisabled();

		ImGui::BeginDisabled(!hasRootOffsetKey);
		if (ImGui::DragFloat3("Visual Offset X / Y / Z", &rootOffsetKey.x, 0.05f))
		{
			statusMessage = SetRootOffsetKeyframe(actionFrame);
		}
		ImGui::EndDisabled();
		ImGui::EndDisabled();
	}

	ImGui::Separator();
	const bool saveRequested = ImGui::Button("Save MotionData", ImVec2(160.0f, 28.0f));

	ImGui::Separator();
	ImGui::Text("Timeline");
	for (int previewFrame = 0; previewFrame <= totalFrames; ++previewFrame)
	{
		const int frame = previewFrame - 1;
		const bool hasKey = frame >= 0 && HasMotionKeyframe(frame);
		const bool hasMove = !editingCommonMotion
			&& frame >= 0
			&& HasAttackMovementKeyframe(attackData, frame);
		const bool hasRoot = editingCommonMotion && frame >= 0 && HasRootOffsetKeyframe(frame);

		ImGui::PushID(previewFrame);
		std::string label;
		if (previewFrame == 0)
		{
			label = "Idle";
		}
		else
		{
			const std::string marker = hasKey && hasMove
				? "*M"
				: hasKey
					? "*"
					: hasMove
						? "M"
						: hasRoot
							? "R"
							: "";
			label = marker + std::to_string(frame);
		}

		const bool selected = previewController.GetCurrentFrame() == previewFrame;
		if (selected)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.95f, 0.75f, 0.10f, 0.85f));
		}
		else if (hasKey && hasMove)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.75f, 0.95f, 0.85f));
		}
		else if (hasKey)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.45f, 0.95f, 0.85f));
		}
		else if (hasMove)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.60f, 0.85f, 0.85f));
		}
		else if (hasRoot)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.70f, 0.35f, 0.85f));
		}

		if (ImGui::Button(label.c_str(), ImVec2(46.0f, 28.0f)))
		{
			previewController.Stop();
			previewController.SetCurrentFrame(previewFrame);
			RefreshFrameEditValues(frame);
			attackMovementKeyOffset = frame >= 0
				? GetAttackMovementOffsetAtFrame(attackData, frame)
				: Vector2::Zero;
		}

		if (selected || hasKey || hasMove || hasRoot)
		{
			ImGui::PopStyleColor();
		}

		if ((previewFrame + 1) % 10 != 0 && previewFrame < totalFrames)
		{
			ImGui::SameLine();
		}
		ImGui::PopID();
	}

	if (!statusMessage.empty())
	{
		ImGui::TextWrapped("%s", statusMessage.c_str());
	}

	return saveRequested;
}

void CustomizeMotionEditorController::DrawPosePresetControls(int keyFrame, int totalFrames, std::string& statusMessage)
{
	(void)totalFrames;

	ImGui::Separator();
	ImGui::Text("Pose Preset");
	ImGui::BeginDisabled(!HasMotionKeyframe(keyFrame));
	if (ImGui::Button("Save Pose Preset", ImVec2(150.0f, 28.0f)))
	{
		showSavePresetPanel = !showSavePresetPanel;
		showLoadPresetPanel = false;
	}
	ImGui::SameLine();
	if (ImGui::Button("Load Pose Preset", ImVec2(150.0f, 28.0f)))
	{
		showLoadPresetPanel = !showLoadPresetPanel;
		showSavePresetPanel = false;
		if (std::none_of(presetApplyMask.begin(), presetApplyMask.end(), [](bool selected) { return selected; }))
		{
			presetApplyMask.fill(true);
		}
	}
	ImGui::EndDisabled();

	if (!HasMotionKeyframe(keyFrame))
	{
		ImGui::TextDisabled("Preset save/load requires a keyframe on the current frame.");
		return;
	}

	if (showSavePresetPanel)
	{
		ImGui::SeparatorText("Save Current Pose Preset");
		ImGui::InputText("Preset Display Name", presetNameBuffer.data(), presetNameBuffer.size());
		if (ImGui::Button("Save Current Pose", ImVec2(160.0f, 28.0f)))
		{
			statusMessage = SaveCurrentPoseAsPreset(keyFrame);
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel Save", ImVec2(120.0f, 28.0f)))
		{
			showSavePresetPanel = false;
		}
	}

	if (showLoadPresetPanel)
	{
		ImGui::SeparatorText("Load Pose Preset");
		const std::vector<PosePresetListItem> presets = BuildPosePresetList();
		if (presets.empty())
		{
			ImGui::TextDisabled("No pose preset found.");
			return;
		}

		selectedPresetIndex = std::clamp(selectedPresetIndex, 0, static_cast<int>(presets.size()) - 1);
		if (ImGui::BeginCombo("Pose Preset", presets[static_cast<size_t>(selectedPresetIndex)].displayName.c_str()))
		{
			for (int presetIndex = 0; presetIndex < static_cast<int>(presets.size()); ++presetIndex)
			{
				const bool selected = selectedPresetIndex == presetIndex;
				if (ImGui::Selectable(presets[static_cast<size_t>(presetIndex)].displayName.c_str(), selected))
				{
					selectedPresetIndex = presetIndex;
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		if (ImGui::Button("Select All Parts", ImVec2(150.0f, 26.0f)))
		{
			presetApplyMask.fill(true);
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear All Parts", ImVec2(150.0f, 26.0f)))
		{
			presetApplyMask.fill(false);
		}

		constexpr int Columns = 3;
		for (int boneIndex = 0; boneIndex < MotionEditorBoneCount; ++boneIndex)
		{
			ImGui::PushID(boneIndex);
			const bool selected = presetApplyMask[static_cast<size_t>(boneIndex)];
			if (ImGui::Selectable(MotionSkeleton::GetBodyPartName(boneIndex), selected, 0, ImVec2(125.0f, 24.0f)))
			{
				presetApplyMask[static_cast<size_t>(boneIndex)] = !selected;
			}
			ImGui::PopID();
			if ((boneIndex + 1) % Columns != 0)
			{
				ImGui::SameLine();
			}
		}

		const bool hasAnySelectedPart = std::any_of(
			presetApplyMask.begin(),
			presetApplyMask.end(),
			[](bool selected)
			{
				return selected;
			});
		ImGui::BeginDisabled(!hasAnySelectedPart);
		if (ImGui::Button("Apply Preset To Selected Parts", ImVec2(240.0f, 28.0f)))
		{
			statusMessage = ApplyPosePreset(
				presets[static_cast<size_t>(selectedPresetIndex)].presetId,
				keyFrame,
				presetApplyMask);
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Cancel Load", ImVec2(120.0f, 28.0f)))
		{
			showLoadPresetPanel = false;
		}
	}
}

void CustomizeMotionEditorController::DrawIkControls(
    CustomizePreviewController& previewController, int keyFrame, std::string& statusMessage)
{
    ImGui::Separator();
    ImGui::Text("IK Edit (World)");
	// 数値欄とギズモは同じSolveIkTargetを使い、保存先も同じ全身姿勢キーにする。
	ConsumeTranslationEdit(previewController, keyFrame);
	ImGui::BeginDisabled(previewController.translationGizmoActive || ikTargetEditing);
	ImGui::Checkbox("Move gizmo (World)", &previewController.translationGizmoEnabled);
	ImGui::EndDisabled();
	if (previewController.translationGizmoActive) return;
    const auto* chain = MotionIk::FindChain(static_cast<MotionBodyPart>(selectedBoneIndex));
    if (!HasMotionKeyframe(keyFrame) || !chain)
    {
        previewController.EndIkSolveSession();
        ikTargetEditing = false;
        hasIkTargetWorldPosition = false;
        ikTargetKeyFrame = ikTargetBodyPartIndex = -1;
        ImGui::TextDisabled("Select an IK joint on a pose keyframe.");
        return;
    }

    if (chain->translationLocked)
    {
        // 固定条件下で直線移動の自由度がない対象には、動かない入力欄を出さない。
        // 既存の回転編集はそのまま使える。背中/腰を勝手に動かす旧チェーンは禁止。
        previewController.EndIkSolveSession();
        ikTargetEditing = false;
        hasIkTargetWorldPosition = false;
        ikTargetKeyFrame = ikTargetBodyPartIndex = -1;
        ImGui::TextDisabled("Translation locked (fixed hierarchy).");
        return;
    }

    // 部位・フレームが変わったら別の編集。以前のドラッグの基準を流用しない。
    if (ikTargetKeyFrame != keyFrame || ikTargetBodyPartIndex != selectedBoneIndex)
    {
        previewController.EndIkSolveSession();
        ikTargetEditing = false;
    }
    // 操作していない間は、回転編集・プリセット適用後も実際の関節位置に同期する。
    // 操作中だけは要求値を保持し、限界で止まった解で入力を上書きしない。
    if (!ikTargetEditing) CaptureCurrentIkTarget(previewController, keyFrame);
    ImGui::BeginDisabled(!hasIkTargetWorldPosition);
    constexpr const char* labels[] = { "World X", "World Y", "World Z" };
    for (int axis = 0; axis < 3; ++axis)
    {
        float value = (&ikTargetWorldPosition.x)[axis];
        const bool changed = ImGui::DragFloat(labels[axis], &value, 0.01f, 0.0f, 0.0f, "%.3f");
        if (ImGui::IsItemActivated())
        {
            BeginIkEdit(previewController, keyFrame);
        }
        if (changed && ikTargetEditing)
        {
            (&ikTargetWorldPosition.x)[axis] = value;
            statusMessage = UpdateIkEdit(previewController, keyFrame, ikTargetWorldPosition);
        }
        if (ImGui::IsItemDeactivated())
        {
            EndIkEdit(previewController);
        }
    }
    ImGui::EndDisabled();
}

