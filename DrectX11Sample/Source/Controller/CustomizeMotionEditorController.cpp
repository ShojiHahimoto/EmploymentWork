#include "Controller/CustomizeMotionEditorController.h"

#include "Data/MotionDataLoader.h"
#include "Data/MotionDataSaver.h"
#include "System/imgui-docking/imgui.h"

#include <DirectXMath.h>

#include <algorithm>
#include <cstdio>

using namespace DirectX::SimpleMath;

namespace
{
	/// <summary>
	/// 編集用部位番号を有効範囲へ収める。
	/// </summary>
	/// <param name="bodyPartIndex">補正する部位番号。</param>
	/// <returns>0 から MotionEditorBoneCount - 1 の部位番号。</returns>
	int ClampBodyPartIndex(int bodyPartIndex)
	{
		return std::clamp(bodyPartIndex, 0, CustomizeMotionEditorController::MotionEditorBoneCount - 1);
	}

	/// <summary>
	/// MotionData 内から指定部位のトラックを検索し、なければ作成する。
	/// </summary>
	/// <param name="motionData">編集対象の MotionData。</param>
	/// <param name="boneName">編集用部位名。</param>
	/// <returns>指定部位のトラック。</returns>
	MotionBoneTrackData* FindOrCreateMotionTrack(MotionData& motionData, const std::string& boneName)
	{
		for (MotionBoneTrackData& track : motionData.boneTracks)
		{
			if (track.boneName == boneName)
			{
				return &track;
			}
		}

		MotionBoneTrackData& newTrack = motionData.boneTracks.emplace_back();
		newTrack.boneName = boneName;
		return &newTrack;
	}

	/// <summary>
	/// MotionData 内から指定部位のトラックを検索する。
	/// </summary>
	/// <param name="motionData">検索対象の MotionData。</param>
	/// <param name="boneName">編集用部位名。</param>
	/// <returns>見つかったトラック。なければ nullptr。</returns>
	const MotionBoneTrackData* FindMotionTrack(const MotionData& motionData, const std::string& boneName)
	{
		for (const MotionBoneTrackData& track : motionData.boneTracks)
		{
			if (track.boneName == boneName)
			{
				return &track;
			}
		}

		return nullptr;
	}

	/// <summary>
	/// トラック内から指定フレームの姿勢キーを検索する。
	/// </summary>
	/// <param name="track">検索対象の部位トラック。</param>
	/// <param name="frame">検索する内部 actionFrame。</param>
	/// <returns>見つかったキー。なければ nullptr。</returns>
	MotionBoneKeyframeData* FindMotionKeyframe(MotionBoneTrackData& track, int frame)
	{
		for (MotionBoneKeyframeData& keyframe : track.keyframes)
		{
			if (keyframe.frame == frame)
			{
				return &keyframe;
			}
		}

		return nullptr;
	}

	/// <summary>
	/// トラック内から指定フレームの姿勢キーを検索する。
	/// </summary>
	/// <param name="track">検索対象の部位トラック。</param>
	/// <param name="frame">検索する内部 actionFrame。</param>
	/// <returns>見つかったキー。なければ nullptr。</returns>
	const MotionBoneKeyframeData* FindMotionKeyframe(const MotionBoneTrackData& track, int frame)
	{
		for (const MotionBoneKeyframeData& keyframe : track.keyframes)
		{
			if (keyframe.frame == frame)
			{
				return &keyframe;
			}
		}

		return nullptr;
	}

	/// <summary>
	/// 汎用モーション用の見た目オフセットキーを検索する。
	/// </summary>
	/// <param name="motionData">検索対象の MotionData。</param>
	/// <param name="frame">検索する内部 actionFrame。</param>
	/// <returns>見つかったキー。なければ nullptr。</returns>
	MotionRootOffsetKeyData* FindMotionRootOffsetKeyframe(MotionData& motionData, int frame)
	{
		for (MotionRootOffsetKeyData& keyframe : motionData.rootOffsetKeys)
		{
			if (keyframe.frame == frame)
			{
				return &keyframe;
			}
		}

		return nullptr;
	}

	/// <summary>
	/// 汎用モーション用の見た目オフセットキーを検索する。
	/// </summary>
	/// <param name="motionData">検索対象の MotionData。</param>
	/// <param name="frame">検索する内部 actionFrame。</param>
	/// <returns>見つかったキー。なければ nullptr。</returns>
	const MotionRootOffsetKeyData* FindMotionRootOffsetKeyframe(const MotionData& motionData, int frame)
	{
		for (const MotionRootOffsetKeyData& keyframe : motionData.rootOffsetKeys)
		{
			if (keyframe.frame == frame)
			{
				return &keyframe;
			}
		}

		return nullptr;
	}

	/// <summary>
	/// AttackData 内から指定フレームの攻撃移動キーを検索する。
	/// </summary>
	/// <param name="attackData">検索対象の AttackData。</param>
	/// <param name="frame">検索する内部 actionFrame。</param>
	/// <returns>見つかったキー。なければ nullptr。</returns>
	AttackMovementKeyData* FindAttackMovementKeyframe(AttackData& attackData, int frame)
	{
		for (AttackMovementKeyData& keyframe : attackData.movementKeys)
		{
			if (keyframe.frame == frame)
			{
				return &keyframe;
			}
		}

		return nullptr;
	}

	/// <summary>
	/// AttackData 内から指定フレームの攻撃移動キーを検索する。
	/// </summary>
	/// <param name="attackData">検索対象の AttackData。</param>
	/// <param name="frame">検索する内部 actionFrame。</param>
	/// <returns>見つかったキー。なければ nullptr。</returns>
	const AttackMovementKeyData* FindAttackMovementKeyframe(const AttackData& attackData, int frame)
	{
		for (const AttackMovementKeyData& keyframe : attackData.movementKeys)
		{
			if (keyframe.frame == frame)
			{
				return &keyframe;
			}
		}

		return nullptr;
	}

	/// <summary>
	/// MotionData から指定部位のローカル回転を取得する。
	/// </summary>
	/// <param name="motionData">参照する MotionData。</param>
	/// <param name="boneName">編集用部位名。</param>
	/// <param name="frame">参照する内部 actionFrame。</param>
	/// <returns>補間済みのオイラー角。</returns>
	Vector3 GetMotionRotationEulerAtFrame(const MotionData& motionData, const std::string& boneName, int frame)
	{
		const MotionBoneTrackData* track = FindMotionTrack(motionData, boneName);
		if (!track || track->keyframes.empty())
		{
			return Vector3::Zero;
		}

		const MotionBoneKeyframeData* previousKey = nullptr;
		const MotionBoneKeyframeData* nextKey = nullptr;
		for (const MotionBoneKeyframeData& keyframe : track->keyframes)
		{
			if (keyframe.frame <= frame)
			{
				previousKey = &keyframe;
			}
			if (keyframe.frame >= frame)
			{
				nextKey = &keyframe;
				break;
			}
		}

		if (!previousKey && nextKey)
		{
			return nextKey->localRotationEulerDegrees;
		}
		if (previousKey && !nextKey)
		{
			return previousKey->localRotationEulerDegrees;
		}
		if (!previousKey || !nextKey || previousKey == nextKey)
		{
			return previousKey ? previousKey->localRotationEulerDegrees : Vector3::Zero;
		}

		const float range = static_cast<float>(std::max(1, nextKey->frame - previousKey->frame));
		const float t = static_cast<float>(frame - previousKey->frame) / range;
		return Vector3::Lerp(previousKey->localRotationEulerDegrees, nextKey->localRotationEulerDegrees, t);
	}

	/// <summary>
	/// MotionData の指定部位とフレームにローカル回転キーを追加または上書きする。
	/// </summary>
	/// <param name="motionData">編集対象の MotionData。</param>
	/// <param name="boneName">編集用部位名。</param>
	/// <param name="frame">設定先の内部 actionFrame。</param>
	/// <param name="rotationEulerDegrees">保存するオイラー角。</param>
	void SetMotionRotationKey(
		MotionData& motionData,
		const std::string& boneName,
		int frame,
		const Vector3& rotationEulerDegrees)
	{
		MotionBoneTrackData* targetTrack = FindOrCreateMotionTrack(motionData, boneName);
		MotionBoneKeyframeData* targetKeyframe = FindMotionKeyframe(*targetTrack, frame);
		if (!targetKeyframe)
		{
			targetKeyframe = &targetTrack->keyframes.emplace_back();
			targetKeyframe->frame = frame;
		}

		targetKeyframe->hasRotation = true;
		targetKeyframe->localRotationEulerDegrees = rotationEulerDegrees;
		targetKeyframe->localRotation = Quaternion::CreateFromYawPitchRoll(
			DirectX::XMConvertToRadians(rotationEulerDegrees.y),
			DirectX::XMConvertToRadians(rotationEulerDegrees.x),
			DirectX::XMConvertToRadians(rotationEulerDegrees.z));
	}

	/// <summary>
	/// MotionData から指定フレームの汎用見た目オフセットを取得する。
	/// </summary>
	/// <param name="motionData">参照する MotionData。</param>
	/// <param name="frame">参照する内部 actionFrame。</param>
	/// <returns>補間済みの見た目オフセット。</returns>
	Vector3 GetMotionRootOffsetAtFrame(const MotionData& motionData, int frame)
	{
		if (motionData.rootOffsetKeys.empty())
		{
			return Vector3::Zero;
		}

		const MotionRootOffsetKeyData* previousKey = nullptr;
		const MotionRootOffsetKeyData* nextKey = nullptr;
		for (const MotionRootOffsetKeyData& keyframe : motionData.rootOffsetKeys)
		{
			if (keyframe.frame <= frame)
			{
				previousKey = &keyframe;
			}
			if (keyframe.frame >= frame)
			{
				nextKey = &keyframe;
				break;
			}
		}

		if (!previousKey && nextKey)
		{
			return Vector3::Lerp(Vector3::Zero, nextKey->offset, 1.0f);
		}
		if (previousKey && !nextKey)
		{
			return previousKey->offset;
		}
		if (!previousKey || !nextKey || previousKey == nextKey)
		{
			return previousKey ? previousKey->offset : Vector3::Zero;
		}

		const float range = static_cast<float>(std::max(1, nextKey->frame - previousKey->frame));
		const float t = static_cast<float>(frame - previousKey->frame) / range;
		return Vector3::Lerp(previousKey->offset, nextKey->offset, t);
	}

	/// <summary>
	/// AttackData から指定フレームの攻撃移動量を補間取得する。
	/// </summary>
	/// <param name="attackData">参照する AttackData。</param>
	/// <param name="frame">参照する内部 actionFrame。</param>
	/// <returns>補間済みの前後/上下移動量。</returns>
	Vector2 GetAttackMovementOffsetAtFrame(const AttackData& attackData, int frame)
	{
		if (attackData.movementKeys.empty())
		{
			return Vector2::Zero;
		}

		const AttackMovementKeyData* previousKey = nullptr;
		const AttackMovementKeyData* nextKey = nullptr;
		for (const AttackMovementKeyData& keyframe : attackData.movementKeys)
		{
			if (keyframe.frame <= frame)
			{
				previousKey = &keyframe;
			}
			if (keyframe.frame >= frame)
			{
				nextKey = &keyframe;
				break;
			}
		}

		if (!previousKey && nextKey)
		{
			return Vector2::Lerp(Vector2::Zero, nextKey->offset, 1.0f);
		}
		if (previousKey && !nextKey)
		{
			return previousKey->offset;
		}
		if (!previousKey || !nextKey || previousKey == nextKey)
		{
			return previousKey ? previousKey->offset : Vector2::Zero;
		}

		const float range = static_cast<float>(std::max(1, nextKey->frame - previousKey->frame));
		const float t = static_cast<float>(frame - previousKey->frame) / range;
		return Vector2::Lerp(previousKey->offset, nextKey->offset, t);
	}

	/// <summary>
	/// MotionData の指定フレームに汎用見た目オフセットキーを追加または上書きする。
	/// </summary>
	/// <param name="motionData">編集対象の MotionData。</param>
	/// <param name="frame">設定先の内部 actionFrame。</param>
	/// <param name="offset">保存する見た目オフセット。</param>
	void SetMotionRootOffsetKey(MotionData& motionData, int frame, const Vector3& offset)
	{
		MotionRootOffsetKeyData* targetKeyframe = FindMotionRootOffsetKeyframe(motionData, frame);
		if (!targetKeyframe)
		{
			targetKeyframe = &motionData.rootOffsetKeys.emplace_back();
			targetKeyframe->frame = frame;
		}

		targetKeyframe->offset = offset;
	}

	/// <summary>
	/// AttackData の指定フレームに攻撃移動キーを追加または上書きする。
	/// </summary>
	/// <param name="attackData">編集対象の AttackData。</param>
	/// <param name="frame">設定先の内部 actionFrame。</param>
	/// <param name="offset">保存する前後/上下移動量。</param>
	void SetAttackMovementKey(AttackData& attackData, int frame, const Vector2& offset)
	{
		AttackMovementKeyData* targetKeyframe = FindAttackMovementKeyframe(attackData, frame);
		if (!targetKeyframe)
		{
			targetKeyframe = &attackData.movementKeys.emplace_back();
			targetKeyframe->frame = frame;
		}

		targetKeyframe->offset = offset;
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
		&& FindAttackMovementKeyframe(attackData, actionFrame) != nullptr;
	const bool hasRootOffsetKey = editingCommonMotion && HasRootOffsetKeyframe(actionFrame);
	const int pickedBodyPartIndex = previewController.ConsumePickedBodyPartIndex();
	if (pickedBodyPartIndex >= 0 && pickedBodyPartIndex < MotionEditorBoneCount)
	{
		selectedBoneIndex = ClampBodyPartIndex(pickedBodyPartIndex);
		if (canEditCurrentFrame)
		{
			RefreshFrameEditValues(actionFrame);
		}
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
	ImGui::SameLine();
	if (ImGui::Button("T Pose", ImVec2(90.0f, 28.0f)))
	{
		statusMessage = ApplyTPosePreset(actionFrame);
	}
	ImGui::EndDisabled();

	ImGui::Separator();
	ImGui::Text("Pose Edit");
	if (!canEditCurrentFrame)
	{
		ImGui::TextDisabled("Add a keyframe to this frame before editing the pose.");
	}

	ImGui::BeginDisabled(!canEditCurrentFrame);
	const int previousBoneIndex = selectedBoneIndex;
	bool partChanged = false;
	selectedBoneIndex = ClampBodyPartIndex(selectedBoneIndex);
	if (ImGui::BeginCombo("Target Part", MotionSkeleton::GetBodyPartName(selectedBoneIndex)))
	{
		for (int bodyPartIndex = 0; bodyPartIndex < MotionEditorBoneCount; ++bodyPartIndex)
		{
			const bool isSelected = selectedBoneIndex == bodyPartIndex;
			if (ImGui::Selectable(MotionSkeleton::GetBodyPartName(bodyPartIndex), isSelected))
			{
				selectedBoneIndex = bodyPartIndex;
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
	if (ImGui::DragFloat3("Rotation Euler Degrees X / Y / Z", &rotationEulerDegrees.x, 0.5f))
	{
		statusMessage = SetSelectedRotationKey(actionFrame, totalFrames);
	}
	ImGui::EndDisabled();

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
			attackMovementKeyOffset = GetAttackMovementOffsetAtFrame(attackData, actionFrame);
			SetAttackMovementKey(attackData, actionFrame, attackMovementKeyOffset);
			statusMessage = "Added attack movement keyframe.";
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(!hasMovementKey);
		if (ImGui::Button("Delete Movement Keyframe", ImVec2(210.0f, 28.0f)))
		{
			attackData.movementKeys.erase(
				std::remove_if(
					attackData.movementKeys.begin(),
					attackData.movementKeys.end(),
					[actionFrame](const AttackMovementKeyData& keyframe)
					{
						return keyframe.frame == actionFrame;
					}),
				attackData.movementKeys.end());
			attackMovementKeyOffset = GetAttackMovementOffsetAtFrame(attackData, actionFrame);
			statusMessage = "Deleted attack movement keyframe.";
		}
		ImGui::EndDisabled();

		ImGui::BeginDisabled(!hasMovementKey);
		if (ImGui::DragFloat2("Attack Movement Offset Forward / Up", &attackMovementKeyOffset.x, 0.05f))
		{
			SetAttackMovementKey(attackData, actionFrame, attackMovementKeyOffset);
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
			&& FindAttackMovementKeyframe(attackData, frame) != nullptr;
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

void CustomizeMotionEditorController::ResetForAttackMotion()
{
	hasDraft = false;
	draft = MotionData();
	CopyEditorBuffers(-1);
}

std::string CustomizeMotionEditorController::LoadDraft(
	const std::string& motionDataId,
	const std::string& fallbackDisplayName,
	int totalFrames,
	bool looping)
{
	if (motionDataId.empty())
	{
		hasDraft = false;
		return "MotionData ID is empty.";
	}

	if (!MotionDataLoader::LoadMotionData(motionDataId, draft))
	{
		draft = MotionData();
		draft.motionDataId = motionDataId;
		draft.displayName = fallbackDisplayName;
		draft.totalFrames = std::max(1, totalFrames);
		draft.looping = looping;
		hasDraft = true;
		CopyEditorBuffers(-1);
		return "New MotionData draft created.";
	}

	draft.totalFrames = std::max(1, totalFrames);
	draft.looping = looping;
	hasDraft = true;
	CopyEditorBuffers(-1);
	return "Loaded existing MotionData.";
}

std::string CustomizeMotionEditorController::SaveDraft(const std::string& motionDataId, int totalFrames, bool looping)
{
	if (!hasDraft)
	{
		return "No MotionData draft.";
	}
	if (motionDataId.empty())
	{
		return "MotionData ID is empty.";
	}

	draft.motionDataId = motionDataId;
	draft.displayName = displayNameBuffer.data();
	draft.totalFrames = std::max(1, totalFrames);
	draft.looping = looping;
	NormalizeDraftForSave();

	if (MotionDataSaver::SaveMotionData(draft.motionDataId, draft))
	{
		MotionDataManager::UnloadAll();
		return "Saved MotionData: assets/MotionData/" + draft.motionDataId + ".json";
	}

	return "MotionData save failed.";
}

void CustomizeMotionEditorController::CopyEditorBuffers(int actionFrame)
{
	displayNameBuffer.fill('\0');
	selectedBoneIndex = 3;
	rotationEulerDegrees = Vector3::Zero;
	rootOffsetKey = Vector3::Zero;

	if (!hasDraft)
	{
		return;
	}

	std::snprintf(displayNameBuffer.data(), displayNameBuffer.size(), "%s", draft.displayName.c_str());
	if (!draft.boneTracks.empty())
	{
		const MotionBoneTrackData& track = draft.boneTracks.front();
		selectedBoneIndex = ClampBodyPartIndex(MotionSkeleton::FindBodyPartIndex(track.boneName));
	}

	RefreshFrameEditValues(actionFrame);
}

void CustomizeMotionEditorController::RefreshFrameEditValues(int actionFrame)
{
	if (!hasDraft)
	{
		rotationEulerDegrees = Vector3::Zero;
		rootOffsetKey = Vector3::Zero;
		return;
	}

	const int sampleFrame = std::max(0, actionFrame);
	selectedBoneIndex = ClampBodyPartIndex(selectedBoneIndex);
	const std::string boneName = MotionSkeleton::GetBodyPartName(selectedBoneIndex);
	rotationEulerDegrees = GetMotionRotationEulerAtFrame(draft, boneName, sampleFrame);
	rootOffsetKey = GetMotionRootOffsetAtFrame(draft, sampleFrame);
}

Vector2 CustomizeMotionEditorController::GetAttackMovementOffsetAtFrame(const AttackData& attackData, int frame) const
{
	return ::GetAttackMovementOffsetAtFrame(attackData, frame);
}

std::string CustomizeMotionEditorController::AddWholeBodyKeyframe(int keyFrame, int totalFrames)
{
	if (!hasDraft)
	{
		return "No MotionData draft.";
	}
	if (keyFrame < 0)
	{
		return "Select preview frame 1 or later before adding a keyframe.";
	}

	draft.totalFrames = std::max(1, totalFrames);
	for (int boneIndex = 0; boneIndex < MotionEditorBoneCount; ++boneIndex)
	{
		const char* boneName = MotionSkeleton::GetBodyPartName(boneIndex);
		const Vector3 rotation = GetMotionRotationEulerAtFrame(draft, boneName, keyFrame);
		SetMotionRotationKey(draft, boneName, keyFrame, rotation);
	}

	RefreshFrameEditValues(keyFrame);
	return "Added whole body MotionData keyframe.";
}

std::string CustomizeMotionEditorController::DeleteWholeBodyKeyframe(int keyFrame)
{
	if (!hasDraft)
	{
		return "No MotionData draft.";
	}
	if (keyFrame < 0)
	{
		return "Select preview frame 1 or later before deleting a keyframe.";
	}

	for (MotionBoneTrackData& track : draft.boneTracks)
	{
		track.keyframes.erase(
			std::remove_if(
				track.keyframes.begin(),
				track.keyframes.end(),
				[keyFrame](const MotionBoneKeyframeData& keyframe)
				{
					return keyframe.frame == keyFrame;
				}),
			track.keyframes.end());
	}
	draft.boneTracks.erase(
		std::remove_if(
			draft.boneTracks.begin(),
			draft.boneTracks.end(),
			[](const MotionBoneTrackData& track)
			{
				return track.keyframes.empty();
			}),
		draft.boneTracks.end());

	return "Deleted current MotionData keyframe.";
}

std::string CustomizeMotionEditorController::SetSelectedRotationKey(int keyFrame, int totalFrames)
{
	if (!hasDraft)
	{
		return "No MotionData draft.";
	}
	if (keyFrame < 0)
	{
		return "Select preview frame 1 or later before setting a MotionData key.";
	}
	if (!HasMotionKeyframe(keyFrame))
	{
		return "Add a whole body keyframe before editing pose.";
	}

	const std::string boneName = MotionSkeleton::GetBodyPartName(selectedBoneIndex);
	draft.totalFrames = std::max(1, totalFrames);
	SetMotionRotationKey(draft, boneName, keyFrame, rotationEulerDegrees);

	MotionBoneTrackData* targetTrack = FindOrCreateMotionTrack(draft, boneName);
	std::sort(
		targetTrack->keyframes.begin(),
		targetTrack->keyframes.end(),
		[](const MotionBoneKeyframeData& left, const MotionBoneKeyframeData& right)
		{
			return left.frame < right.frame;
		});

	return "Set MotionData keyframe.";
}

bool CustomizeMotionEditorController::HasMotionKeyframe(int keyFrame) const
{
	if (!hasDraft || keyFrame < 0)
	{
		return false;
	}

	for (const MotionBoneTrackData& track : draft.boneTracks)
	{
		if (FindMotionKeyframe(track, keyFrame))
		{
			return true;
		}
	}

	return false;
}

std::string CustomizeMotionEditorController::AddRootOffsetKeyframe(int keyFrame)
{
	if (!hasDraft)
	{
		return "No MotionData draft.";
	}
	if (keyFrame < 0)
	{
		return "Select preview frame 1 or later before adding a visual offset keyframe.";
	}

	rootOffsetKey = GetMotionRootOffsetAtFrame(draft, keyFrame);
	SetMotionRootOffsetKey(draft, keyFrame, rootOffsetKey);
	return "Added common motion visual offset keyframe.";
}

std::string CustomizeMotionEditorController::DeleteRootOffsetKeyframe(int keyFrame)
{
	if (!hasDraft)
	{
		return "No MotionData draft.";
	}
	if (keyFrame < 0)
	{
		return "Select preview frame 1 or later before deleting a visual offset keyframe.";
	}

	draft.rootOffsetKeys.erase(
		std::remove_if(
			draft.rootOffsetKeys.begin(),
			draft.rootOffsetKeys.end(),
			[keyFrame](const MotionRootOffsetKeyData& keyframe)
			{
				return keyframe.frame == keyFrame;
			}),
		draft.rootOffsetKeys.end());
	rootOffsetKey = GetMotionRootOffsetAtFrame(draft, keyFrame);
	return "Deleted common motion visual offset keyframe.";
}

std::string CustomizeMotionEditorController::SetRootOffsetKeyframe(int keyFrame)
{
	if (!hasDraft)
	{
		return "No MotionData draft.";
	}
	if (keyFrame < 0)
	{
		return "Select preview frame 1 or later before setting a visual offset keyframe.";
	}
	if (!HasRootOffsetKeyframe(keyFrame))
	{
		return "Add a visual offset keyframe before editing common motion offset.";
	}

	SetMotionRootOffsetKey(draft, keyFrame, rootOffsetKey);
	return "Set common motion visual offset keyframe.";
}

bool CustomizeMotionEditorController::HasRootOffsetKeyframe(int keyFrame) const
{
	if (!hasDraft || keyFrame < 0)
	{
		return false;
	}

	return FindMotionRootOffsetKeyframe(draft, keyFrame) != nullptr;
}

std::string CustomizeMotionEditorController::CopyWholeBodyPose(int keyFrame)
{
	if (!HasMotionKeyframe(keyFrame))
	{
		return "Copy requires a keyframe on the current frame.";
	}

	for (int boneIndex = 0; boneIndex < MotionEditorBoneCount; ++boneIndex)
	{
		const std::string boneName = MotionSkeleton::GetBodyPartName(boneIndex);
		copiedPoseRotations[boneIndex] = GetMotionRotationEulerAtFrame(draft, boneName, keyFrame);
	}

	hasCopiedPose = true;
	return "Copied whole body pose.";
}

std::string CustomizeMotionEditorController::PasteWholeBodyPose(int keyFrame)
{
	if (!hasCopiedPose)
	{
		return "No copied pose.";
	}
	if (!HasMotionKeyframe(keyFrame))
	{
		return "Paste requires a keyframe on the current frame.";
	}

	for (int boneIndex = 0; boneIndex < MotionEditorBoneCount; ++boneIndex)
	{
		const std::string boneName = MotionSkeleton::GetBodyPartName(boneIndex);
		SetMotionRotationKey(draft, boneName, keyFrame, copiedPoseRotations[boneIndex]);
	}

	RefreshFrameEditValues(keyFrame);
	return "Pasted whole body pose.";
}

std::string CustomizeMotionEditorController::ApplyTPosePreset(int keyFrame)
{
	if (!HasMotionKeyframe(keyFrame))
	{
		return "T Pose requires a keyframe on the current frame.";
	}

	for (int boneIndex = 0; boneIndex < MotionEditorBoneCount; ++boneIndex)
	{
		const std::string boneName = MotionSkeleton::GetBodyPartName(boneIndex);
		SetMotionRotationKey(draft, boneName, keyFrame, Vector3::Zero);
	}

	RefreshFrameEditValues(keyFrame);
	return "Applied T Pose preset.";
}

MotionData& CustomizeMotionEditorController::GetDraft()
{
	return draft;
}

const MotionData& CustomizeMotionEditorController::GetDraft() const
{
	return draft;
}

bool CustomizeMotionEditorController::HasDraft() const
{
	return hasDraft;
}

std::array<char, CustomizeMotionEditorController::MotionNameBufferSize>&
CustomizeMotionEditorController::GetDisplayNameBuffer()
{
	return displayNameBuffer;
}

int& CustomizeMotionEditorController::GetSelectedBoneIndex()
{
	return selectedBoneIndex;
}

Vector3& CustomizeMotionEditorController::GetRotationEulerDegrees()
{
	return rotationEulerDegrees;
}

Vector3& CustomizeMotionEditorController::GetRootOffsetKey()
{
	return rootOffsetKey;
}

bool CustomizeMotionEditorController::HasCopiedPose() const
{
	return hasCopiedPose;
}

void CustomizeMotionEditorController::NormalizeDraftForSave()
{
	const int lastFrame = std::max(0, draft.totalFrames - 1);
	for (MotionBoneTrackData& track : draft.boneTracks)
	{
		for (MotionBoneKeyframeData& keyframe : track.keyframes)
		{
			keyframe.frame = std::clamp(keyframe.frame, 0, lastFrame);
		}

		std::sort(
			track.keyframes.begin(),
			track.keyframes.end(),
			[](const MotionBoneKeyframeData& left, const MotionBoneKeyframeData& right)
			{
				return left.frame < right.frame;
			});
	}
	draft.boneTracks.erase(
		std::remove_if(
			draft.boneTracks.begin(),
			draft.boneTracks.end(),
			[](const MotionBoneTrackData& track)
			{
				return track.keyframes.empty();
			}),
		draft.boneTracks.end());

	for (MotionRootOffsetKeyData& keyframe : draft.rootOffsetKeys)
	{
		keyframe.frame = std::clamp(keyframe.frame, 0, lastFrame);
	}
	std::sort(
		draft.rootOffsetKeys.begin(),
		draft.rootOffsetKeys.end(),
		[](const MotionRootOffsetKeyData& left, const MotionRootOffsetKeyData& right)
		{
			return left.frame < right.frame;
		});
}
