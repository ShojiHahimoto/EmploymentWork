#include "Controller/CustomizeMotionEditorController.h"

#include "Data/MotionDataLoader.h"
#include "Controller/KeyframeEditing.h"
#include "System/MotionRotationLimits.h"
#include "Data/MotionDataSaver.h"
#include "Data/PosePreset.h"


#include <DirectXMath.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <vector>

using namespace DirectX::SimpleMath;

namespace
{
    constexpr const char* IdlePosePresetId = "__IdlePose__";
    Vector3 QuaternionToEulerDegrees(const Quaternion& rotation);
    Vector3 GetMotionRotationEulerAtFrame(const MotionData& motionData, const std::string& boneName, int frame);
    Quaternion GetMotionRotationQuaternionAtFrame(const MotionData& motionData, const std::string& boneName, int frame);

	/// <summary>
	/// 文字列が空白だけか確認する。
	/// </summary>
	/// <param name="text">確認する文字列。</param>
	/// <returns>空または空白だけなら true。</returns>
	bool IsBlank(const std::string& text)
	{
		return std::all_of(
			text.begin(),
			text.end(),
			[](unsigned char character)
			{
				return std::isspace(character) != 0;
			});
	}

	/// <summary>
	/// Common/Idle の 0F 姿勢を、プリセットと同じ形に変換する。
	/// </summary>
	/// <param name="outPreset">作成した Idle 姿勢プリセットの書き込み先。</param>
	/// <returns>Idle MotionData の読み込みに成功した場合は true。</returns>
	bool BuildIdlePosePreset(PosePresetData& outPreset)
	{
		MotionData idleMotion;
		if (!MotionDataLoader::LoadMotionData("Common/Idle", idleMotion))
		{
			return false;
		}

		outPreset = PosePresetData();
		outPreset.presetId = IdlePosePresetId;
		outPreset.displayName = "Idle Pose";
		for (int boneIndex = 0; boneIndex < CustomizeMotionEditorController::MotionEditorBoneCount; ++boneIndex)
		{
			const std::string boneName = MotionSkeleton::GetBodyPartName(boneIndex);
			PosePresetBoneData bone;
			bone.boneName = boneName;
			bone.localRotation = GetMotionRotationQuaternionAtFrame(idleMotion, boneName, 0);
			bone.localRotationEulerDegrees = QuaternionToEulerDegrees(bone.localRotation);
			outPreset.bones.push_back(bone);
		}

		return true;
	}

	/// <summary>
	/// 指定プリセット内から部位回転を検索する。
	/// </summary>
	/// <param name="preset">検索対象のプリセット。</param>
	/// <param name="boneName">検索する編集用部位名。</param>
	/// <param name="outRotation">見つかった回転の書き込み先。</param>
	/// <returns>部位回転が見つかった場合は true。</returns>
	bool FindPresetBoneRotation(
		const PosePresetData& preset,
		const std::string& boneName,
		Quaternion& outRotation)
	{
		for (const PosePresetBoneData& bone : preset.bones)
		{
			if (bone.boneName == boneName)
			{
				outRotation = bone.localRotation;
				return true;
			}
		}

		return false;
	}

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
	/// Vector3 の各軸へ符号を掛ける。
	/// </summary>
	/// <param name="value">変換元の値。</param>
	/// <param name="sign">各軸へ掛ける符号。</param>
	/// <returns>符号変換後の値。</returns>
	Vector3 ApplyAxisSign(const Vector3& value, const Vector3& sign)
	{
		return Vector3(value.x * sign.x, value.y * sign.y, value.z * sign.z);
	}

	/// <summary>
	/// MotionData 内部保存値を、左右対称入力しやすい編集UI表示値へ変換する。
	/// </summary>
	/// <param name="bodyPartIndex">編集用部位番号。</param>
	/// <param name="internalRotationEulerDegrees">MotionData に保存されている内部 Euler 回転。</param>
	/// <returns>編集UIに表示する Euler 回転。</returns>
	Vector3 ConvertInternalRotationToEditorRotation(int bodyPartIndex, const Vector3& internalRotationEulerDegrees)
	{
		const Vector3 sign = MotionSkeleton::GetEditorRotationSign(ClampBodyPartIndex(bodyPartIndex));
		return ApplyAxisSign(internalRotationEulerDegrees, sign);
	}

	/// <summary>
	/// 編集UI表示値を、既存MotionData形式の内部保存値へ戻す。
	/// </summary>
	/// <param name="bodyPartIndex">編集用部位番号。</param>
	/// <param name="editorRotationEulerDegrees">編集UI上の Euler 回転。</param>
	/// <returns>MotionData に保存する内部 Euler 回転。</returns>
	Vector3 ConvertEditorRotationToInternalRotation(int bodyPartIndex, const Vector3& editorRotationEulerDegrees)
	{
		const Vector3 sign = MotionSkeleton::GetEditorRotationSign(ClampBodyPartIndex(bodyPartIndex));
		return ApplyAxisSign(editorRotationEulerDegrees, sign);
	}

	/// <summary>
	/// Quaternion を UI 表示用の Euler degree に変換する。
	/// </summary>
	/// <param name="rotation">変換する Quaternion。</param>
	/// <returns>degree 単位の Euler 回転。</returns>
	Vector3 QuaternionToEulerDegrees(const Quaternion& rotation)
	{
		const Vector3 eulerRadians = rotation.ToEuler();
		return Vector3(
			DirectX::XMConvertToDegrees(eulerRadians.x),
			DirectX::XMConvertToDegrees(eulerRadians.y),
			DirectX::XMConvertToDegrees(eulerRadians.z));
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
		return QuaternionToEulerDegrees(GetMotionRotationQuaternionAtFrame(motionData, boneName, frame));
	}

	/// <summary>
	/// MotionData から指定部位のローカル Quaternion 回転を取得する。
	/// </summary>
	/// <param name="motionData">参照する MotionData。</param>
	/// <param name="boneName">編集用部位名。</param>
	/// <param name="frame">参照する内部 actionFrame。</param>
	/// <returns>補間済みの Quaternion 回転。</returns>
	Quaternion GetMotionRotationQuaternionAtFrame(const MotionData& motionData, const std::string& boneName, int frame)
	{
		const MotionBoneTrackData* track = FindMotionTrack(motionData, boneName);
		if (!track || track->keyframes.empty())
		{
			return Quaternion::Identity;
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
			return nextKey->localRotation;
		}
		if (previousKey && !nextKey)
		{
			return previousKey->localRotation;
		}
		if (!previousKey || !nextKey || previousKey == nextKey)
		{
			return previousKey ? previousKey->localRotation : Quaternion::Identity;
		}

		const float range = static_cast<float>(std::max(1, nextKey->frame - previousKey->frame));
		const float t = static_cast<float>(frame - previousKey->frame) / range;
		Quaternion sampledRotation = Quaternion::Slerp(previousKey->localRotation, nextKey->localRotation, t);
		sampledRotation.Normalize();
		return sampledRotation;
	}

	/// <summary>
	/// MotionData の指定部位とフレームに Quaternion ローカル回転キーを追加または上書きする。
	/// </summary>
	/// <param name="motionData">編集対象の MotionData。</param>
	/// <param name="boneName">編集用部位名。</param>
	/// <param name="frame">設定先の内部 actionFrame。</param>
	/// <param name="localRotation">保存する Quaternion 回転。</param>
	void SetMotionRotationQuaternionKey(
		MotionData& motionData,
		const std::string& boneName,
		int frame,
		const Quaternion& localRotation)
	{
		MotionBoneTrackData* targetTrack = FindOrCreateMotionTrack(motionData, boneName);
		auto& targetKeyframe = KeyframeEditing::FindOrInsert(targetTrack->keyframes, frame);

		Quaternion normalizedRotation = localRotation;
		normalizedRotation.Normalize();
		targetKeyframe.hasRotation = true;
		targetKeyframe.localRotation = normalizedRotation;
		targetKeyframe.localRotationEulerDegrees = QuaternionToEulerDegrees(normalizedRotation);
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
		KeyframeEditing::FindOrInsert(motionData.rootOffsetKeys, frame).offset = offset;
	}

	/// <summary>
	/// AttackData の指定フレームに攻撃移動キーを追加または上書きする。
	/// </summary>
	/// <param name="attackData">編集対象の AttackData。</param>
	/// <param name="frame">設定先の内部 actionFrame。</param>
	/// <param name="offset">保存する前後/上下移動量。</param>
	void SetAttackMovementKey(AttackData& attackData, int frame, const Vector2& offset)
	{
		KeyframeEditing::FindOrInsert(attackData.movementKeys, frame).offset = offset;
	}
}

void CustomizeMotionEditorController::AddAttackMovementKeyframe(AttackData& attackData, int frame, Vector2& offset)
{
	if (!hasDraft || frame < 0) return;
	offset = GetAttackMovementOffsetAtFrame(attackData, frame);
	SetAttackMovementKey(attackData, frame, offset);
}

void CustomizeMotionEditorController::DeleteAttackMovementKeyframe(AttackData& attackData, int frame, Vector2& offset)
{
	if (!hasDraft || frame < 0) return;
	std::erase_if(attackData.movementKeys, [frame](const AttackMovementKeyData& key) { return key.frame == frame; });
	offset = GetAttackMovementOffsetAtFrame(attackData, frame);
}

void CustomizeMotionEditorController::UpdateAttackMovementKeyframe(AttackData& attackData, int frame, const Vector2& offset)
{
	if (hasDraft && HasAttackMovementKeyframe(attackData, frame)) SetAttackMovementKey(attackData, frame, offset);
}

bool CustomizeMotionEditorController::HasAttackMovementKeyframe(const AttackData& attackData, int frame) const
{
	return frame >= 0 && FindAttackMovementKeyframe(attackData, frame) != nullptr;
}

void CustomizeMotionEditorController::SelectBodyPart(CustomizePreviewController& preview, int part, int frame)
{
	EndIkEdit(preview);
	if (part < 0 || part >= MotionEditorBoneCount)
	{
		selectedBoneIndex = -1;
		selectedBodyParts.fill(false);
	}
	else if (multipleSelectionEnabled)
	{
		selectedBodyParts[part] = !selectedBodyParts[part];
		if (selectedBodyParts[part]) selectedBoneIndex = part;
		else if (selectedBoneIndex == part)
		{
			const auto next = std::find(selectedBodyParts.begin(), selectedBodyParts.end(), true);
			selectedBoneIndex = next == selectedBodyParts.end() ? -1 : static_cast<int>(next - selectedBodyParts.begin());
		}
	}
	else selectedBoneIndex = part;
	hasIkTargetWorldPosition = false;
	if (selectedBoneIndex >= 0 && HasMotionKeyframe(frame)) RefreshFrameEditValues(frame);
}

std::array<bool, MotionBodyPartCount> CustomizeMotionEditorController::GetSelectedBodyParts() const
{
	if (multipleSelectionEnabled) return selectedBodyParts;
	std::array<bool, MotionBodyPartCount> selected{};
	if (selectedBoneIndex >= 0 && selectedBoneIndex < MotionBodyPartCount) selected[selectedBoneIndex] = true;
	return selected;
}

void CustomizeMotionEditorController::SetMultipleSelectionEnabled(bool enabled)
{
	if (multipleSelectionEnabled == enabled) return;
	selectedBodyParts.fill(false);
	if (selectedBoneIndex >= 0 && selectedBoneIndex < MotionBodyPartCount) selectedBodyParts[selectedBoneIndex] = true;
	multipleSelectionEnabled = enabled;
}

void CustomizeMotionEditorController::SelectAllBodyParts()
{
	if (!multipleSelectionEnabled) return;
	selectedBodyParts.fill(true);
	if (selectedBoneIndex < 0 || selectedBoneIndex >= MotionBodyPartCount) selectedBoneIndex = 0;
}

bool CustomizeMotionEditorController::BeginIkEdit(CustomizePreviewController& preview, int frame)
{
	EndIkEdit(preview);
	if (!HasMotionKeyframe(frame) || selectedBoneIndex < 0) return false;
	const auto* chain = MotionIk::FindChain(static_cast<MotionBodyPart>(selectedBoneIndex));
	if (!chain || chain->translationLocked) return false;
	CaptureCurrentIkTarget(preview, frame);
	if (!hasIkTargetWorldPosition) return false;
	preview.Stop();
	ikTargetEditing = preview.BeginIkSolveSession(frame);
	return ikTargetEditing;
}

std::string CustomizeMotionEditorController::UpdateIkEdit(CustomizePreviewController& preview, int frame, const Vector3& target)
{
	// 開始時の部位・フレームと一致しない要求は、古い開始姿勢で解かない。
	if (!ikTargetEditing || frame != ikTargetKeyFrame || selectedBoneIndex != ikTargetBodyPartIndex)
		return "Start an IK edit on the current pose keyframe first.";
	ikTargetWorldPosition = target;
	return ApplySelectedIkTarget(preview, frame);
}

void CustomizeMotionEditorController::EndIkEdit(CustomizePreviewController& preview)
{
	preview.EndIkSolveSession();
	ikTargetEditing = false;
}

void CustomizeMotionEditorController::ConsumeTranslationEdit(CustomizePreviewController& preview, int frame)
{
	if (!preview.hasTranslationGizmoEdit) return;
	preview.hasTranslationGizmoEdit = false;
	const auto* chain = MotionIk::FindChain(static_cast<MotionBodyPart>(preview.translationGizmoPart));
	if (!chain || preview.translationGizmoFrame != frame || !HasMotionKeyframe(frame)) return;
	for (int i = 0; i < 3; ++i)
		SetMotionRotationQuaternionKey(draft, MotionSkeleton::GetBodyPartName(static_cast<int>(chain->parts[i])),
			frame, preview.translationGizmoResult.localRotations[i]);
	RefreshFrameEditValues(frame);
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
	multipleSelectionEnabled = false;
	selectedBodyParts.fill(false);
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

void CustomizeMotionEditorController::RestoreDraft(const MotionData& restored, int actionFrame)
{
	draft = restored;
	hasDraft = true;
	std::snprintf(displayNameBuffer.data(), displayNameBuffer.size(), "%s", draft.displayName.c_str());
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
	if (selectedBoneIndex < 0)
	{
		rotationEulerDegrees = Vector3::Zero;
		rootOffsetKey = GetMotionRootOffsetAtFrame(draft, sampleFrame);
		return;
	}
	selectedBoneIndex = ClampBodyPartIndex(selectedBoneIndex);
	const std::string boneName = MotionSkeleton::GetBodyPartName(selectedBoneIndex);
	const Vector3 internalRotationEulerDegrees = GetMotionRotationEulerAtFrame(draft, boneName, sampleFrame);
	rotationEulerDegrees = ConvertInternalRotationToEditorRotation(selectedBoneIndex, internalRotationEulerDegrees);
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
		const Quaternion rotation = GetMotionRotationQuaternionAtFrame(draft, boneName, keyFrame);
		SetMotionRotationQuaternionKey(draft, boneName, keyFrame, rotation);
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

std::string CustomizeMotionEditorController::SetSelectedRotationKey(CustomizePreviewController& previewController, int keyFrame, int totalFrames)
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
	const Vector3 input = ConvertEditorRotationToInternalRotation(selectedBoneIndex, rotationEulerDegrees);
	const Quaternion requested = Quaternion::CreateFromYawPitchRoll(
		DirectX::XMConvertToRadians(input.y), DirectX::XMConvertToRadians(input.x), DirectX::XMConvertToRadians(input.z));
	Quaternion bind;
	if (!previewController.GetBodyPartBindRotation(selectedBoneIndex, bind)) return "Waiting for preview model.";
	const Quaternion previous = GetMotionRotationQuaternionAtFrame(draft, boneName, keyFrame);
	const Quaternion constrained = MotionRotationLimits::Constrain(static_cast<MotionBodyPart>(selectedBoneIndex), previous, requested, bind);
	SetMotionRotationQuaternionKey(draft, boneName, keyFrame, constrained);
	rotationEulerDegrees = ConvertInternalRotationToEditorRotation(selectedBoneIndex, QuaternionToEulerDegrees(constrained));

	return "Set MotionData keyframe.";
}

std::string CustomizeMotionEditorController::ApplyGizmoRotationEdit(
	int bodyPartIndex,
	int axisIndex,
	int keyFrame,
	int totalFrames,
	const Quaternion& localRotation)
{
	if (!hasDraft)
	{
		return "No MotionData draft.";
	}
	if (keyFrame < 0)
	{
		return "Select preview frame 1 or later before editing pose.";
	}
	if (!HasMotionKeyframe(keyFrame))
	{
		return "Add a whole body keyframe before editing pose.";
	}

	selectedBoneIndex = ClampBodyPartIndex(bodyPartIndex);
	if (axisIndex < 0 || axisIndex > 2)
	{
		return "Invalid gizmo rotation axis.";
	}

	const std::string boneName = MotionSkeleton::GetBodyPartName(selectedBoneIndex);
	draft.totalFrames = std::max(1, totalFrames);
	SetMotionRotationQuaternionKey(draft, boneName, keyFrame, localRotation);

	rotationEulerDegrees = ConvertInternalRotationToEditorRotation(
		selectedBoneIndex,
		QuaternionToEulerDegrees(localRotation));
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
		copiedPoseRotations[boneIndex] = GetMotionRotationQuaternionAtFrame(draft, boneName, keyFrame);
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
		SetMotionRotationQuaternionKey(draft, boneName, keyFrame, copiedPoseRotations[boneIndex]);
	}

	RefreshFrameEditValues(keyFrame);
	return "Pasted whole body pose.";
}

std::string CustomizeMotionEditorController::CaptureCurrentIkTarget(
    CustomizePreviewController& previewController, int keyFrame)
{
    hasIkTargetWorldPosition = MotionIk::FindChain(static_cast<MotionBodyPart>(selectedBoneIndex))
        && previewController.HasRenderedActionFrame(keyFrame)
        && previewController.GetBodyPartWorldPosition(selectedBoneIndex, ikTargetWorldPosition);
    ikTargetKeyFrame = hasIkTargetWorldPosition ? keyFrame : -1;
    ikTargetBodyPartIndex = hasIkTargetWorldPosition ? selectedBoneIndex : -1;
    return hasIkTargetWorldPosition ? "IK position synchronized." : "Waiting for preview pose.";
}

std::string CustomizeMotionEditorController::ApplySelectedIkTarget(
    CustomizePreviewController& previewController, int keyFrame)
{
    if (!HasMotionKeyframe(keyFrame) || !hasIkTargetWorldPosition) return "IK requires a pose keyframe.";
    const auto* chain = MotionIk::FindChain(static_cast<MotionBodyPart>(selectedBoneIndex));
    MotionIkResult result;
    if (!chain || !previewController.SolveIkTarget(selectedBoneIndex, ikTargetWorldPosition, result))
        return "IK could not verify the pose. The existing keyframe was kept.";
    if (result.translationLocked) return "IK translation is locked by the fixed hierarchy.";

    // 位置キーは追加しない。親2点の回転を変更し、手足自身のローカル回転は維持する。
    // 実FKで位置解との一致が確認された場合にだけ下書きを変更する。
    for (int i = 0; i < 3; ++i)
        SetMotionRotationQuaternionKey(draft,
            MotionSkeleton::GetBodyPartName(static_cast<int>(chain->parts[i])), keyFrame, result.localRotations[i]);
    RefreshFrameEditValues(keyFrame);
    if (result.atJointLimit) return "IK: joint rotation limit reached on the selected axis.";
    return result.atReachLimit ? "IK: reached the limit on the selected axis." : "IK pose updated.";
}

std::string CustomizeMotionEditorController::SavePosePreset(int keyFrame, const std::string& displayName)
{
	if (!HasMotionKeyframe(keyFrame))
	{
		return "Pose preset save requires a keyframe on the current frame.";
	}

	if (IsBlank(displayName))
	{
		return "Preset name is empty.";
	}
	if (displayName == "Idle Pose" || PosePresetStore::DisplayNameExists(displayName))
	{
		return "Preset display name already exists.";
	}

	const std::string presetId = PosePresetStore::CreateUniquePresetId();
	PosePresetData preset;
	preset.presetId = presetId;
	preset.displayName = displayName;
	for (int boneIndex = 0; boneIndex < MotionEditorBoneCount; ++boneIndex)
	{
		const std::string boneName = MotionSkeleton::GetBodyPartName(boneIndex);
		PosePresetBoneData bone;
		bone.boneName = boneName;
		bone.localRotation = GetMotionRotationQuaternionAtFrame(draft, boneName, keyFrame);
		bone.localRotationEulerDegrees = QuaternionToEulerDegrees(bone.localRotation);
		preset.bones.push_back(bone);
	}

	if (!PosePresetStore::SavePreset(presetId, preset))
	{
		return "Failed to save pose preset.";
	}

	return "Saved pose preset: " + displayName;
}

std::string CustomizeMotionEditorController::ApplyPosePreset(
	const std::string& presetId,
	int keyFrame,
	const std::array<bool, MotionEditorBoneCount>& applyMask)
{
	if (!HasMotionKeyframe(keyFrame))
	{
		return "Pose preset load requires a keyframe on the current frame.";
	}

	PosePresetData preset;
	if (presetId == IdlePosePresetId)
	{
		if (!BuildIdlePosePreset(preset))
		{
			return "Failed to load Idle pose from Common/Idle.";
		}
	}
	else if (!PosePresetStore::LoadPreset(presetId, preset))
	{
		return "Failed to load pose preset: " + presetId;
	}

	int appliedCount = 0;
	for (int boneIndex = 0; boneIndex < MotionEditorBoneCount; ++boneIndex)
	{
		if (!applyMask[static_cast<size_t>(boneIndex)])
		{
			continue;
		}

		const std::string boneName = MotionSkeleton::GetBodyPartName(boneIndex);
		Quaternion rotation = Quaternion::Identity;
		if (FindPresetBoneRotation(preset, boneName, rotation))
		{
			SetMotionRotationQuaternionKey(draft, boneName, keyFrame, rotation);
			++appliedCount;
		}
	}

	RefreshFrameEditValues(keyFrame);
	return appliedCount > 0
		? "Applied pose preset: " + preset.displayName
		: "Pose preset has no matching selected parts.";
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
