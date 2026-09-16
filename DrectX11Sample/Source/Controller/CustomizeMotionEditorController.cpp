#include "Controller/CustomizeMotionEditorController.h"

#include "Data/MotionDataLoader.h"
#include "Data/MotionDataSaver.h"
#include "Data/PosePreset.h"
#include "System/imgui-docking/imgui.h"

#include <DirectXMath.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <vector>

using namespace DirectX::SimpleMath;

namespace
{
	/// <summary>
	/// 読み込み UI に表示する姿勢プリセット候補。
	/// </summary>
	struct PosePresetListItem
	{
		std::string presetId;
		std::string displayName;
		bool idleMotion = false;
	};

	constexpr const char* IdlePosePresetId = "__IdlePose__";

	Vector3 GetMotionRotationEulerAtFrame(const MotionData& motionData, const std::string& boneName, int frame);

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
			bone.localRotationEulerDegrees = GetMotionRotationEulerAtFrame(idleMotion, boneName, 0);
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
		Vector3& outRotation)
	{
		for (const PosePresetBoneData& bone : preset.bones)
		{
			if (bone.boneName == boneName)
			{
				outRotation = bone.localRotationEulerDegrees;
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
	/// 内部可動域を編集UI表示用の可動域へ変換する。
	/// </summary>
	/// <param name="bodyPartIndex">編集用部位番号。</param>
	/// <param name="minDegrees">UI表示用の最小値。</param>
	/// <param name="maxDegrees">UI表示用の最大値。</param>
	void GetEditorRotationLimitRange(int bodyPartIndex, Vector3& minDegrees, Vector3& maxDegrees)
	{
		const MotionJointRotationLimit& limit = MotionSkeleton::GetRotationLimit(ClampBodyPartIndex(bodyPartIndex));
		minDegrees = limit.minDegrees;
		maxDegrees = limit.maxDegrees;
		if (!limit.enabled)
		{
			return;
		}

		const Vector3 sign = MotionSkeleton::GetEditorRotationSign(ClampBodyPartIndex(bodyPartIndex));
		if (sign.x < 0.0f)
		{
			std::swap(minDegrees.x, maxDegrees.x);
			minDegrees.x = -minDegrees.x;
			maxDegrees.x = -maxDegrees.x;
		}
		if (sign.y < 0.0f)
		{
			std::swap(minDegrees.y, maxDegrees.y);
			minDegrees.y = -minDegrees.y;
			maxDegrees.y = -maxDegrees.y;
		}
		if (sign.z < 0.0f)
		{
			std::swap(minDegrees.z, maxDegrees.z);
			minDegrees.z = -minDegrees.z;
			maxDegrees.z = -maxDegrees.z;
		}
	}

	/// <summary>
	/// 部位ごとの可動域に合わせてローカル回転を補正する。
	/// </summary>
	/// <param name="bodyPartIndex">編集用部位番号。</param>
	/// <param name="rotationEulerDegrees">補正する Euler 回転。</param>
	/// <returns>制限内へ補正した Euler 回転。制限なし部位は入力値そのまま。</returns>
	Vector3 ClampRotationByBodyPart(int bodyPartIndex, const Vector3& rotationEulerDegrees)
	{
		const MotionJointRotationLimit& limit = MotionSkeleton::GetRotationLimit(ClampBodyPartIndex(bodyPartIndex));
		if (!limit.enabled)
		{
			return rotationEulerDegrees;
		}

		return Vector3(
			std::clamp(rotationEulerDegrees.x, limit.minDegrees.x, limit.maxDegrees.x),
			std::clamp(rotationEulerDegrees.y, limit.minDegrees.y, limit.maxDegrees.y),
			std::clamp(rotationEulerDegrees.z, limit.minDegrees.z, limit.maxDegrees.z));
	}

	/// <summary>
	/// 編集UI表示値を、表示上の可動域に合わせて補正する。
	/// </summary>
	/// <param name="bodyPartIndex">編集用部位番号。</param>
	/// <param name="editorRotationEulerDegrees">編集UI上の Euler 回転。</param>
	/// <returns>編集UI表示用可動域に収めた Euler 回転。</returns>
	Vector3 ClampEditorRotationByBodyPart(int bodyPartIndex, const Vector3& editorRotationEulerDegrees)
	{
		const MotionJointRotationLimit& limit = MotionSkeleton::GetRotationLimit(ClampBodyPartIndex(bodyPartIndex));
		if (!limit.enabled)
		{
			return editorRotationEulerDegrees;
		}

		Vector3 editorMin;
		Vector3 editorMax;
		GetEditorRotationLimitRange(bodyPartIndex, editorMin, editorMax);
		return Vector3(
			std::clamp(editorRotationEulerDegrees.x, editorMin.x, editorMax.x),
			std::clamp(editorRotationEulerDegrees.y, editorMin.y, editorMax.y),
			std::clamp(editorRotationEulerDegrees.z, editorMin.z, editorMax.z));
	}

	/// <summary>
	/// 部位名に対応する可動域でローカル回転を補正する。
	/// </summary>
	/// <param name="boneName">編集用部位名。</param>
	/// <param name="rotationEulerDegrees">補正する Euler 回転。</param>
	/// <returns>制限内へ補正した Euler 回転。</returns>
	Vector3 ClampRotationByBoneName(const std::string& boneName, const Vector3& rotationEulerDegrees)
	{
		const int bodyPartIndex = MotionSkeleton::FindBodyPartIndex(boneName);
		if (bodyPartIndex < 0)
		{
			return rotationEulerDegrees;
		}

		return ClampRotationByBodyPart(bodyPartIndex, rotationEulerDegrees);
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

		const Vector3 clampedRotationEulerDegrees = ClampRotationByBoneName(boneName, rotationEulerDegrees);
		targetKeyframe->hasRotation = true;
		targetKeyframe->localRotationEulerDegrees = clampedRotationEulerDegrees;
		targetKeyframe->localRotation = Quaternion::CreateFromYawPitchRoll(
			DirectX::XMConvertToRadians(clampedRotationEulerDegrees.y),
			DirectX::XMConvertToRadians(clampedRotationEulerDegrees.x),
			DirectX::XMConvertToRadians(clampedRotationEulerDegrees.z));
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
	const MotionJointRotationLimit& selectedLimit = MotionSkeleton::GetRotationLimit(selectedBoneIndex);
	if (selectedLimit.enabled)
	{
		Vector3 editorLimitMin;
		Vector3 editorLimitMax;
		GetEditorRotationLimitRange(selectedBoneIndex, editorLimitMin, editorLimitMax);
		ImGui::Text(
			"Rotation Limit X %.0f..%.0f / Y %.0f..%.0f / Z %.0f..%.0f",
			editorLimitMin.x,
			editorLimitMax.x,
			editorLimitMin.y,
			editorLimitMax.y,
			editorLimitMin.z,
			editorLimitMax.z);
	}
	else
	{
		ImGui::Text("Rotation Limit: None");
	}
	if (ImGui::DragFloat3("Rotation Euler Degrees X / Y / Z", &rotationEulerDegrees.x, 0.5f))
	{
		rotationEulerDegrees = ClampEditorRotationByBodyPart(selectedBoneIndex, rotationEulerDegrees);
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
	rotationEulerDegrees = ClampEditorRotationByBodyPart(selectedBoneIndex, rotationEulerDegrees);
	const Vector3 internalRotationEulerDegrees =
		ConvertEditorRotationToInternalRotation(selectedBoneIndex, rotationEulerDegrees);
	SetMotionRotationKey(draft, boneName, keyFrame, internalRotationEulerDegrees);

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

std::string CustomizeMotionEditorController::SaveCurrentPoseAsPreset(int keyFrame)
{
	if (!HasMotionKeyframe(keyFrame))
	{
		return "Pose preset save requires a keyframe on the current frame.";
	}

	const std::string displayName = presetNameBuffer.data();
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
		bone.localRotationEulerDegrees = GetMotionRotationEulerAtFrame(draft, boneName, keyFrame);
		preset.bones.push_back(bone);
	}

	if (!PosePresetStore::SavePreset(presetId, preset))
	{
		return "Failed to save pose preset.";
	}

	presetNameBuffer.fill('\0');
	showSavePresetPanel = false;
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
		Vector3 rotation = Vector3::Zero;
		if (FindPresetBoneRotation(preset, boneName, rotation))
		{
			SetMotionRotationKey(draft, boneName, keyFrame, rotation);
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
