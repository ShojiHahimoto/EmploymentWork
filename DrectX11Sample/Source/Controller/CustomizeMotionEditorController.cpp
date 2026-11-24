#include "Controller/CustomizeMotionEditorController.h"

#pragma execution_character_set("utf-8")

#include "Customize/KeyframeEditing.h"
#include "Data/MotionDataLoader.h"
#include "Customize/MotionPose.h"
#include "Data/MotionDataSaver.h"
#include "Data/PosePreset.h"
#include "Customize/MotionRotationLimits.h"

#include <DirectXMath.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <vector>

using namespace DirectX::SimpleMath;

namespace
{
constexpr const char *IdlePosePresetId = "__IdlePose__";

Vector3 QuaternionToEulerDegrees(const Quaternion &rotation);
Quaternion GetMotionRotationQuaternionAtFrame(const MotionData &motionData, const std::string &boneName, int frame);

/// <summary>
/// 文字列が空白だけか確認する。
/// </summary>
/// <param name="text">確認する文字列。</param>
/// <returns>空または空白だけなら true。</returns>
bool IsBlank(const std::string &text)
{
	return std::all_of(text.begin(), text.end(),
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
bool BuildIdlePosePreset(PosePresetData &outPreset)
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
bool FindPresetBoneRotation(const PosePresetData &preset, const std::string &boneName, Quaternion &outRotation)
{
	for (const PosePresetBoneData &bone : preset.bones)
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
Vector3 ApplyAxisSign(const Vector3 &value, const Vector3 &sign)
{
	return Vector3(value.x * sign.x, value.y * sign.y, value.z * sign.z);
}

/// <summary>
/// MotionData 内部保存値を、左右対称入力しやすい編集UI表示値へ変換する。
/// </summary>
/// <param name="bodyPartIndex">編集用部位番号。</param>
/// <param name="internalRotationEulerDegrees">MotionData に保存されている内部 Euler 回転。</param>
/// <returns>編集UIに表示する Euler 回転。</returns>
Vector3 ConvertInternalRotationToEditorRotation(int bodyPartIndex, const Vector3 &internalRotationEulerDegrees)
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
Vector3 ConvertEditorRotationToInternalRotation(int bodyPartIndex, const Vector3 &editorRotationEulerDegrees)
{
	const Vector3 sign = MotionSkeleton::GetEditorRotationSign(ClampBodyPartIndex(bodyPartIndex));

	return ApplyAxisSign(editorRotationEulerDegrees, sign);
}

/// <summary>
/// Quaternion を UI 表示用の Euler degree に変換する。
/// </summary>
/// <param name="rotation">変換する Quaternion。</param>
/// <returns>degree 単位の Euler 回転。</returns>
Vector3 QuaternionToEulerDegrees(const Quaternion &rotation)
{
	const Vector3 eulerRadians = rotation.ToEuler();

	return Vector3(DirectX::XMConvertToDegrees(eulerRadians.x), DirectX::XMConvertToDegrees(eulerRadians.y),
				   DirectX::XMConvertToDegrees(eulerRadians.z));
}

/// <summary>
/// MotionData 内から指定部位のトラックを検索し、なければ作成する。
/// </summary>
/// <param name="motionData">編集対象の MotionData。</param>
/// <param name="boneName">編集用部位名。</param>
/// <returns>指定部位のトラック。</returns>
MotionBoneTrackData *FindOrCreateMotionTrack(MotionData &motionData, const std::string &boneName)
{
	for (MotionBoneTrackData &track : motionData.boneTracks)
	{
		if (track.boneName == boneName)
		{
			return &track;
		}
	}

	MotionBoneTrackData &newTrack = motionData.boneTracks.emplace_back();
	newTrack.boneName = boneName;

	return &newTrack;
}

/// <summary>
/// MotionData 内から指定部位のトラックを検索する。
/// </summary>
/// <param name="motionData">検索対象の MotionData。</param>
/// <param name="boneName">編集用部位名。</param>
/// <returns>見つかったトラック。なければ nullptr。</returns>
const MotionBoneTrackData *FindMotionTrack(const MotionData &motionData, const std::string &boneName)
{
	for (const MotionBoneTrackData &track : motionData.boneTracks)
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
MotionBoneKeyframeData *FindMotionKeyframe(MotionBoneTrackData &track, int frame)
{
	for (MotionBoneKeyframeData &keyframe : track.keyframes)
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
const MotionBoneKeyframeData *FindMotionKeyframe(const MotionBoneTrackData &track, int frame)
{
	for (const MotionBoneKeyframeData &keyframe : track.keyframes)
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
MotionRootOffsetKeyData *FindMotionRootOffsetKeyframe(MotionData &motionData, int frame)
{
	for (MotionRootOffsetKeyData &keyframe : motionData.rootOffsetKeys)
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
const MotionRootOffsetKeyData *FindMotionRootOffsetKeyframe(const MotionData &motionData, int frame)
{
	for (const MotionRootOffsetKeyData &keyframe : motionData.rootOffsetKeys)
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
AttackMovementKeyData *FindAttackMovementKeyframe(AttackData &attackData, int frame)
{
	for (AttackMovementKeyData &keyframe : attackData.movementKeys)
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
const AttackMovementKeyData *FindAttackMovementKeyframe(const AttackData &attackData, int frame)
{
	for (const AttackMovementKeyData &keyframe : attackData.movementKeys)
	{
		if (keyframe.frame == frame)
		{
			return &keyframe;
		}
	}

	return nullptr;
}

/// <summary>
/// MotionData から指定部位のローカル Quaternion 回転を取得する。
/// </summary>
/// <param name="motionData">参照する MotionData。</param>
/// <param name="boneName">編集用部位名。</param>
/// <param name="frame">参照する内部 actionFrame。</param>
/// <returns>補間済みの Quaternion 回転。</returns>
Quaternion GetMotionRotationQuaternionAtFrame(const MotionData &motionData, const std::string &boneName, int frame)
{
	const auto* track = FindMotionTrack(motionData, boneName);
	const BonePose base;

	return track ? MotionPose::SampleBoneTrack(*track, base, frame, motionData.totalFrames,
		motionData.looping).localRotation : base.localRotation;
}

/// <summary>
/// MotionData の指定部位とフレームに Quaternion ローカル回転キーを追加または上書きする。
/// </summary>
/// <param name="motionData">編集対象の MotionData。</param>
/// <param name="boneName">編集用部位名。</param>
/// <param name="frame">設定先の内部 actionFrame。</param>
/// <param name="localRotation">保存する Quaternion 回転。</param>
void SetMotionRotationQuaternionKey(MotionData &motionData, const std::string &boneName, int frame,
									const Quaternion &localRotation)
{
	MotionBoneTrackData *targetTrack = FindOrCreateMotionTrack(motionData, boneName);

	auto &targetKeyframe = KeyframeEditing::FindOrInsert(targetTrack->keyframes, frame);

	Quaternion normalizedRotation = localRotation;
	normalizedRotation.Normalize();
	targetKeyframe.hasRotation = true;
	targetKeyframe.localRotation = normalizedRotation;
	targetKeyframe.localRotationEulerDegrees = QuaternionToEulerDegrees(normalizedRotation);
}

/// <summary>
/// MotionData から指定フレームのオフセット（汎用モーション用の見た目だけの移動）を取得する。
/// </summary>
/// <param name="motionData">参照する MotionData。</param>
/// <param name="frame">参照する内部 actionFrame。</param>
/// <returns>補間済みの見た目オフセット。</returns>
Vector3 GetMotionRootOffsetAtFrame(const MotionData &motionData, int frame)
{
	if (motionData.rootOffsetKeys.empty())
	{
		return Vector3::Zero;
	}

	const MotionRootOffsetKeyData *previousKey = nullptr;
	const MotionRootOffsetKeyData *nextKey = nullptr;

	//1つ前と後ろのキーフレームを取得する
	for (const MotionRootOffsetKeyData &keyframe : motionData.rootOffsetKeys)
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

	//先頭より前なら０～先頭キー間で補完
	if (!previousKey && nextKey)
	{
		return Vector3::Lerp(Vector3::Zero, nextKey->offset, 1.0f);
	}

	//最後尾より後ろなら最後尾を返す
	if (previousKey && !nextKey)
	{
		return previousKey->offset;
	}

	//キーフレーム上ならそのまま返す、１つもキーがなければ０
	if (!previousKey || !nextKey || previousKey == nextKey)
	{
		return previousKey ? previousKey->offset : Vector3::Zero;
	}

	//間なら補完して返す
	const float range = static_cast<float>(std::max(1, nextKey->frame - previousKey->frame));
	const float t = static_cast<float>(frame - previousKey->frame) / range;

	return Vector3::Lerp(previousKey->offset, nextKey->offset, t);
}

/// <summary>
/// AttackData から移動キーフレームの移動量を取得する。
/// </summary>
/// <param name="attackData">参照する AttackData。</param>
/// <param name="frame">参照する内部 actionFrame。</param>
/// <returns>補間済みの前後/上下移動量。</returns>
Vector2 GetAttackMovementOffsetAtFrame(const AttackData &attackData, int frame)
{
	if (attackData.movementKeys.empty())
	{
		return Vector2::Zero;
	}

	const AttackMovementKeyData *previousKey = nullptr;
	const AttackMovementKeyData *nextKey = nullptr;

	//前のキーと後ろのキーを取得
	for (const AttackMovementKeyData &keyframe : attackData.movementKeys)
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

	//先頭より前なら先頭に向けて補完
	if (!previousKey && nextKey)
	{
		return Vector2::Lerp(Vector2::Zero, nextKey->offset, 1.0f);
	}

	//最後尾より後ろなら最後尾
	if (previousKey && !nextKey)
	{
		return previousKey->offset;
	}

	//指定フレームにキーがあるならそのまま、１つもなければ０
	if (!previousKey || !nextKey || previousKey == nextKey)
	{
		return previousKey ? previousKey->offset : Vector2::Zero;
	}

	//間なら補完する
	const float range = static_cast<float>(std::max(1, nextKey->frame - previousKey->frame));	//キー間の幅
	const float t = static_cast<float>(frame - previousKey->frame) / range;		//割合

	return Vector2::Lerp(previousKey->offset, nextKey->offset, t);
}

/// <summary>
/// MotionData の指定フレームに汎用見た目オフセットキーを追加または上書きする。
/// </summary>
/// <param name="motionData">編集対象の MotionData。</param>
/// <param name="frame">設定先の内部 actionFrame。</param>
/// <param name="offset">保存する見た目オフセット。</param>
void SetMotionRootOffsetKey(MotionData &motionData, int frame, const Vector3 &offset)
{
	KeyframeEditing::FindOrInsert(motionData.rootOffsetKeys, frame).offset = offset;
}

/// <summary>
/// AttackData の指定フレームに攻撃移動キーを追加または上書きする。
/// </summary>
/// <param name="attackData">編集対象の AttackData。</param>
/// <param name="frame">設定先の内部 actionFrame。</param>
/// <param name="offset">保存する前後/上下移動量。</param>
void SetAttackMovementKey(AttackData &attackData, int frame, const Vector2 &offset)
{
	KeyframeEditing::FindOrInsert(attackData.movementKeys, frame).offset = offset;
}
} // namespace

/// <summary>UIに依存せず攻撃移動キーを追加する。既存の補間位置を初期値にする。</summary>
/// <param name="attackData">編集対象。</param>
/// <param name="frame">内部フレーム。</param>
/// <param name="offset">追加した位置の出力先。</param>
void CustomizeMotionEditorController::AddAttackMovementKeyframe(AttackData &attackData, int frame, Vector2 &offset)
{
	if (!hasDraft || frame < 0)
	{
		return;
	}

	offset = GetAttackMovementOffsetAtFrame(attackData, frame);
	SetAttackMovementKey(attackData, frame, offset);
}

/// <summary>指定フレームの攻撃移動キーを削除し、編集値を再取得する。</summary>
/// <param name="attackData">編集対象。</param>
/// <param name="frame">内部フレーム。</param>
/// <param name="offset">削除後の補間位置。</param>
void CustomizeMotionEditorController::DeleteAttackMovementKeyframe(AttackData &attackData, int frame, Vector2 &offset)
{
	if (!hasDraft || frame < 0)
	{
		return;
	}

	std::erase_if(attackData.movementKeys,
				  [frame](const AttackMovementKeyData &key)
				  {
					  return key.frame == frame;
				  });
	offset = GetAttackMovementOffsetAtFrame(attackData, frame);
}

/// <summary>既存の攻撃移動キーだけを更新する。姿勢キーとは独立して扱う。</summary>
/// <param name="attackData">編集対象。</param>
/// <param name="frame">内部フレーム。</param>
/// <param name="offset">前後・上下の移動量。</param>
void CustomizeMotionEditorController::UpdateAttackMovementKeyframe(AttackData &attackData, int frame,
																   const Vector2 &offset)
{
	if (hasDraft && HasAttackMovementKeyframe(attackData, frame))
	{
		SetAttackMovementKey(attackData, frame, offset);
	}
}

/// <summary>指定フレームに攻撃移動キーがあるか取得する。</summary>
/// <param name="attackData">参照対象。</param>
/// <param name="frame">内部フレーム。</param>
/// <returns>キーがあればtrue。</returns>
bool CustomizeMotionEditorController::HasAttackMovementKeyframe(const AttackData &attackData, int frame) const
{
	return frame >= 0 && FindAttackMovementKeyframe(attackData, frame) != nullptr;
}

/// <summary>選択部位と編集値を更新し、新たに選んだ部位がIKで移動できない場合は回転モードに戻す。</summary>
/// <param name="preview">プレビュー制御。</param>
/// <param name="part">部位番号。-1は選択解除。</param>
/// <param name="frame">内部フレーム。</param>
void CustomizeMotionEditorController::SelectBodyPart(CustomizePreviewController &preview, int part, int frame)
{
	//それ以前のIK編集があるなら終了する
	EndIkEdit(preview);

	//部位以外の場所をクリックされたら選択解除
	if (part < 0 || part >= MotionEditorBoneCount)
	{
		selectedBoneIndex = -1;
		selectedBodyParts.fill(false);
	}
	//複数選択モードの場合
	else if (multipleSelectionEnabled)
	{
		//選択済みなら解除、未選択なら選択
		selectedBodyParts[part] = !selectedBodyParts[part];

		//直近に選択された部位をメインの選択対象に登録
		//メインの選択対象＝角度編集できる対象
		if (selectedBodyParts[part])
		{
			selectedBoneIndex = part;
		}
		//選択されたのがメインの部位なら他の部位をメインに登録する
		else if (selectedBoneIndex == part)
		{
			const auto next = std::find(selectedBodyParts.begin(), selectedBodyParts.end(), true);
			selectedBoneIndex =
				next == selectedBodyParts.end() ? -1 : static_cast<int>(next - selectedBodyParts.begin());
		}
	}
	//単一選択モードの場合そのまま選択
	else
	{
		selectedBoneIndex = part;
	}

	if (preview.translationGizmoEnabled && part >= 0 && part == selectedBoneIndex)
	{
		const auto* chain = MotionIk::FindChain(static_cast<MotionBodyPart>(part));
		if (!chain || chain->translationLocked)
		{
			preview.translationGizmoEnabled = false;
		}
	}

	hasIkTargetWorldPosition = false;

	if (selectedBoneIndex >= 0 && HasMotionKeyframe(frame))
	{
		RefreshFrameEditValues(frame);
	}
}

/// <summary>プリセット等の対象に使う選択部位を取得する。Gizmoのアクティブ部位とは分ける。</summary>
/// <returns>選択された部位だけtrue。</returns>
std::array<bool, MotionBodyPartCount> CustomizeMotionEditorController::GetSelectedBodyParts() const
{
	//複数選択モードなら選択している部位全てを返す
	if (multipleSelectionEnabled)
	{
		return selectedBodyParts;
	}

	std::array<bool, MotionBodyPartCount> selected{};

	//単一選択モードなら選択している部位だけを返す。
	if (selectedBoneIndex >= 0 && selectedBoneIndex < MotionBodyPartCount)
	{
		selected[selectedBoneIndex] = true;
	}

	return selected;
}

/// <summary>モード切替時はアクティブ部位を基準に選択を同期する。</summary>
/// <param name="enabled">複数選択を使う場合true。</param>
void CustomizeMotionEditorController::SetMultipleSelectionEnabled(bool enabled)
{
	if (multipleSelectionEnabled == enabled)
	{
		return;
	}

	selectedBodyParts.fill(false);

	if (selectedBoneIndex >= 0 && selectedBoneIndex < MotionBodyPartCount)
	{
		selectedBodyParts[selectedBoneIndex] = true;
	}

	multipleSelectionEnabled = enabled;
}

/// <summary>全ての編集部位を選択する。一括のGizmo変形は行わない。</summary>
void CustomizeMotionEditorController::SelectAllBodyParts()
{
	if (!multipleSelectionEnabled)
	{
		return;
	}

	selectedBodyParts.fill(true);

	if (selectedBoneIndex < 0 || selectedBoneIndex >= MotionBodyPartCount)
	{
		selectedBoneIndex = 0;
	}
}

/// <summary>UI・ギズモ共通のIK編集開始。位置は一時目標であり保存しない。</summary>
/// <param name="preview">姿勢・IK計算を提供するプレビュー。</param>
/// <param name="frame">姿勢キーのある内部フレーム。</param>
/// <returns>編集セッションを開始できた場合true。</returns>
bool CustomizeMotionEditorController::BeginIkEdit(CustomizePreviewController &preview, int frame)
{
	//以前のIK操作を終了する
	EndIkEdit(preview);

	//キーフレームがない、選択中でない場合終了
	if (!HasMotionKeyframe(frame) || selectedBoneIndex < 0)
	{
		return false;
	}

	//回転させる選択された部位とその親２点を取得
	const auto *chain = MotionIk::FindChain(static_cast<MotionBodyPart>(selectedBoneIndex));

	if (!chain || chain->translationLocked)
	{
		return false;
	}

	//選択した部位のワールド座標を取得
	CaptureCurrentIkTarget(preview, frame);

	if (!hasIkTargetWorldPosition)
	{
		return false;
	}

	//プレビュー再生を停止
	preview.Stop();

	//IK計算の基準値として、開始時のワールド座標を保存しておく
	ikTargetEditing = preview.BeginIkSolveSession(frame);

	return ikTargetEditing;
}

/// <summary>ワールド位置の要求をIKで解き、回転キーへ反映する。</summary>
/// <param name="preview">IK計算を提供するプレビュー。</param>
/// <param name="frame">開始時と同じ内部フレーム。</param>
/// <param name="target">ワールド基準の目標位置。</param>
/// <returns>操作結果。</returns>
std::string CustomizeMotionEditorController::UpdateIkEdit(CustomizePreviewController &preview, int frame,
														  const Vector3 &target)
{
	// 開始時の部位・フレームと一致しない要求は、古い開始姿勢で解かない。
	if (!ikTargetEditing || frame != ikTargetKeyFrame || selectedBoneIndex != ikTargetBodyPartIndex)
	{
		return "Start an IK edit on the current pose keyframe first.";
	}

	ikTargetWorldPosition = target;

	return ApplySelectedIkTarget(preview, frame);
}

/// <summary>IKの開始姿勢を解放し、次の操作へ持ち越さない。</summary>
/// <param name="preview">終了するプレビュー。</param>
void CustomizeMotionEditorController::EndIkEdit(CustomizePreviewController &preview)
{
	preview.EndIkSolveSession();
	ikTargetEditing = false;
}

/// <summary>旧ギズモの確定済みIK結果を回転キーへ取り込む。描画APIには依存しない。</summary>
/// <param name="preview">操作結果を持つプレビュー。</param>
/// <param name="frame">反映先の内部フレーム。</param>
void CustomizeMotionEditorController::ConsumeTranslationEdit(CustomizePreviewController &preview, int frame)
{
	if (!preview.hasTranslationGizmoEdit)
	{
		return;
	}

	preview.hasTranslationGizmoEdit = false;

	const auto *chain = MotionIk::FindChain(static_cast<MotionBodyPart>(preview.translationGizmoPart));

	if (!chain || preview.translationGizmoFrame != frame || !HasMotionKeyframe(frame))
	{
		return;
	}

	for (int i = 0; i < 3; ++i)
	{
		SetMotionRotationQuaternionKey(draft, MotionSkeleton::GetBodyPartName(static_cast<int>(chain->parts[i])), frame,
									   preview.translationGizmoResult.localRotations[i]);
	}

	RefreshFrameEditValues(frame);
}

/// <summary>攻撃モーション編集用に、MotionData 下書きと編集バッファを初期化する。</summary>
void CustomizeMotionEditorController::ResetForAttackMotion()
{
	hasDraft = false;
	draft = MotionData();
	CopyEditorBuffers(-1);
}

/// <summary>MotionData ID から編集用下書きを読み込み、存在しなければ新規下書きを作る。</summary>
/// <param name="motionDataId">assets/MotionData 配下の拡張子なし ID。</param>
/// <param name="fallbackDisplayName">新規下書き時の表示名。</param>
/// <param name="totalFrames">新規作成時の総フレーム数。攻撃では読み込み成功時も技の尺に同期する。</param>
/// <param name="commonMotion">汎用ならtrueで保存済みの尺とループ設定を維持する。新規汎用の初期ループ値はtrue。</param>
/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
std::string CustomizeMotionEditorController::LoadDraft(const std::string &motionDataId,
													   const std::string &fallbackDisplayName, int totalFrames,
													   bool commonMotion)
{
	//モーションデータIDが無い場合失敗
	if (motionDataId.empty())
	{
		hasDraft = false;

		return "MotionData ID is empty.";
	}

	//モーションデータの下書きが無い場合新規作成
	if (!MotionDataLoader::LoadMotionData(motionDataId, draft))
	{
		draft = MotionData();
		draft.motionDataId = motionDataId;
		draft.displayName = fallbackDisplayName;
		draft.totalFrames = std::max(1, totalFrames);
		draft.looping = commonMotion;
		hasDraft = true;
		CopyEditorBuffers(-1);

		return "New MotionData draft created.";
	}

	// 汎用モーションの保存値は維持する。攻撃だけ技データの尺・非ループへ同期する。
	if (!commonMotion)
	{
		draft.totalFrames = std::max(1, totalFrames);
		draft.looping = false;
	}

	hasDraft = true;
	CopyEditorBuffers(-1);

	return "Loaded existing MotionData.";
}

/// <summary>編集中の MotionData 下書きを保存する。</summary>
/// <param name="motionDataId">保存先 MotionData ID。</param>
/// <param name="totalFrames">保存時に確定する総フレーム数。</param>
/// <param name="looping">ループモーションとして保存する場合は true。</param>
/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
std::string CustomizeMotionEditorController::SaveDraft(const std::string &motionDataId, int totalFrames, bool looping)
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

	//assets/MotionData配下のjsonとして保存する
	if (MotionDataSaver::SaveMotionData(draft.motionDataId, draft))
	{
		//元データを破棄
		MotionDataManager::UnloadAll();

		return "Saved MotionData: assets/MotionData/" + draft.motionDataId + ".json";
	}

	//保存に失敗した場合
	return "MotionData save failed.";
}

/// <summary>現在の下書き内容を UI 共通の入力用バッファへコピーする。</summary>
/// <param name="actionFrame">現在の内部 actionFrame。0F Idle の場合は -1。</param>
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
		const MotionBoneTrackData &track = draft.boneTracks.front();
		selectedBoneIndex = ClampBodyPartIndex(MotionSkeleton::FindBodyPartIndex(track.boneName));
	}

	RefreshFrameEditValues(actionFrame);
}

/// <summary>履歴から下書きを復元し、表示用入力値を同期する。コピー済み姿勢や選択部位は維持する。</summary>
/// <param name="restored">復元するモーション。</param>
/// <param name="actionFrame">表示する内部フレーム。</param>
void CustomizeMotionEditorController::RestoreDraft(const MotionData &restored, int actionFrame)
{
	draft = restored;
	hasDraft = true;
	std::snprintf(displayNameBuffer.data(), displayNameBuffer.size(), "%s", draft.displayName.c_str());
	RefreshFrameEditValues(actionFrame);
}

/// <summary>選択部位やフレームが変わった時、現在フレームの編集値をバッファへ反映する。</summary>
/// <param name="actionFrame">現在の内部 actionFrame。0F Idle の場合は -1。</param>
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
	const Vector3 internalRotationEulerDegrees = QuaternionToEulerDegrees(
		GetMotionRotationQuaternionAtFrame(draft, boneName, sampleFrame));
	rotationEulerDegrees = ConvertInternalRotationToEditorRotation(selectedBoneIndex, internalRotationEulerDegrees);
	rootOffsetKey = GetMotionRootOffsetAtFrame(draft, sampleFrame);
}

/// <summary>AttackData から指定フレームの攻撃移動量を補間取得する。</summary>
/// <param name="attackData">参照する AttackData。</param>
/// <param name="frame">参照する内部 actionFrame。</param>
/// <returns>補間済みの前後/上下移動量。</returns>
Vector2 CustomizeMotionEditorController::GetAttackMovementOffsetAtFrame(const AttackData &attackData, int frame) const
{
	return ::GetAttackMovementOffsetAtFrame(attackData, frame);
}

/// <summary>モデルのbind回転と技の基準姿勢を、編集時の共通サンプリングに設定する。</summary>
/// <param name="preview">部位ごとのモデルbind回転を提供するプレビュー。
/// モデルの各部位のbind回転を取得するプレビュー。</param>
/// <param name="attack">技編集ならtrue。汎用モーションではfalseで末尾復帰を無効にする。</param>
/// <param name="usableState">地上は待機、空中はジャンプループの0Fを基準とする発動条件。</param>
/// <remarks>編集と再生で同じ基準姿勢を使う。画面更新前に呼ぶ。</remarks>
void CustomizeMotionEditorController::ConfigurePoseSampling(const CustomizePreviewController& preview,
	bool attack, AttackUsableState usableState)
{
	sampleAttackEndpoints = attack;
	const MotionData* baseMotion = nullptr;
	const char* baseId = MotionPose::GetAttackBaseMotionId(usableState);
	if (attack && MotionDataManager::LoadMotionData(baseId))
	{
		baseMotion = MotionDataManager::GetMotionData(baseId);
	}

	for (int index = 0; index < MotionEditorBoneCount; ++index)
	{
		Quaternion rotation = Quaternion::Identity;
		preview.GetBodyPartBindRotation(index, rotation);
		BonePose base;
		base.localRotation = rotation;

		if (baseMotion)
		{
			const auto* track = FindMotionTrack(*baseMotion, MotionSkeleton::GetBodyPartName(index));
			if (track)
			{
				base = MotionPose::SampleBoneTrack(*track, base, 0, baseMotion->totalFrames, baseMotion->looping);
			}
		}

		samplingBaseRotations[index] = base.localRotation;
	}
}

/// <summary>描画と共通の補間計算で、指定フレームの部位回転を取得する。</summary>
/// <param name="motion">取得元モーション。
/// 取得元の下書き。</param>
/// <param name="boneName">編集対象の部位名。</param>
/// <param name="frame">内部0始まりの取得フレーム。</param>
/// <returns>設定済み基準姿勢から補間したローカル回転。
/// 設定済みの基準姿勢を使った補間済み回転。</returns>
/// <remarks>描画と共通の補間計算から部位回転を取得する。</remarks>
Quaternion CustomizeMotionEditorController::GetMotionRotationQuaternionAtFrame(
	const MotionData& motion, const std::string& boneName, int frame) const
{
	BonePose base;
	for (int index = 0; index < MotionEditorBoneCount; ++index)
	{
		if (boneName == MotionSkeleton::GetBodyPartName(index))
		{
			base.localRotation = samplingBaseRotations[index];
			break;
		}
	}

	const auto* track = FindMotionTrack(motion, boneName);

	return track ? MotionPose::SampleBoneTrack(*track, base, frame, motion.totalFrames,
		motion.looping, sampleAttackEndpoints).localRotation : base.localRotation;
}

/// <summary>追加前の補間済み回転を全身分取得してから、同じフレームに全身キーを保存する。</summary>
/// <param name="keyFrame">追加先の内部0始まりフレーム。負値は追加不可。
/// 追加先の内部 actionFrame。</param>
/// <param name="totalFrames">末尾補間にも使用する技またはモーションの総フレーム数。</param>
/// <returns>追加結果のメッセージ。
/// ユーザー表示用の処理結果メッセージ。</returns>
/// <remarks>追加前の補間済み回転を全身分取得してから、同じフレームへ全身キーを保存する。</remarks>
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

	std::array<Quaternion, MotionEditorBoneCount> rotations;
	for (int index = 0; index < MotionEditorBoneCount; ++index)
	{
		rotations[index] = GetMotionRotationQuaternionAtFrame(draft, MotionSkeleton::GetBodyPartName(index), keyFrame);
	}

	//全身分ループしてキーフレームを追加
	//現状全身単位の追加の導線しか残していないので、部位単体単位の追加を残す意味はないかも
	for (int boneIndex = 0; boneIndex < MotionEditorBoneCount; ++boneIndex)
	{
		const char *boneName = MotionSkeleton::GetBodyPartName(boneIndex);
		const Quaternion rotation = rotations[boneIndex];
		SetMotionRotationQuaternionKey(draft, boneName, keyFrame, rotation);
	}

	RefreshFrameEditValues(keyFrame);

	return "Added whole body MotionData keyframe.";
}

/// <summary>現在フレームの全身姿勢キーフレームを削除する。</summary>
/// <param name="keyFrame">削除対象の内部 actionFrame。</param>
/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
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

	//追加と同様、現状全身単位の導線しか残していないのでこれを前提にしてもいいかも
	for (MotionBoneTrackData &track : draft.boneTracks)
	{
		track.keyframes.erase(std::remove_if(track.keyframes.begin(), track.keyframes.end(),
											 [keyFrame](const MotionBoneKeyframeData &keyframe)
											 {
												 return keyframe.frame == keyFrame;
											 }),
							  track.keyframes.end());
	}

	draft.boneTracks.erase(std::remove_if(draft.boneTracks.begin(), draft.boneTracks.end(),
										  [](const MotionBoneTrackData &track)
										  {
											  return track.keyframes.empty();
										  }),
						   draft.boneTracks.end());

	return "Deleted current MotionData keyframe.";
}

/// <summary>選択部位の回転キーを現在の編集値で更新する。</summary>
/// <param name="previewController">実FBXの基準回転を取得するプレビュー。</param>
/// <param name="keyFrame">更新対象の内部 actionFrame。</param>
/// <param name="totalFrames">MotionData の総フレーム数。</param>
/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
std::string CustomizeMotionEditorController::SetSelectedRotationKey(CustomizePreviewController &previewController,
																	int keyFrame, int totalFrames)
{
	if (!hasDraft)
	{
		return "No MotionData draft.";
	}

	//０フレームは待機状態部分、１からが該当モーション部分
	if (keyFrame < 0)
	{
		return "Select preview frame 1 or later before setting a MotionData key.";
	}

	//キーが無い場合
	if (!HasMotionKeyframe(keyFrame))
	{
		return "Add a whole body keyframe before editing pose.";
	}

	//選択部位名を取得
	const std::string boneName = MotionSkeleton::GetBodyPartName(selectedBoneIndex);
	draft.totalFrames = std::max(1, totalFrames);

	//UI上から直接数値を入力する際の導線。imgui時の機能なので現状の実装では使用していない
	const Vector3 input = ConvertEditorRotationToInternalRotation(selectedBoneIndex, rotationEulerDegrees);
	const Quaternion requested =
		Quaternion::CreateFromYawPitchRoll(DirectX::XMConvertToRadians(input.y), DirectX::XMConvertToRadians(input.x),
										   DirectX::XMConvertToRadians(input.z));
	Quaternion bind;

	if (!previewController.GetBodyPartBindRotation(selectedBoneIndex, bind))
	{
		return "Waiting for preview model.";
	}

	const Quaternion previous = GetMotionRotationQuaternionAtFrame(draft, boneName, keyFrame);
	const Quaternion constrained =
		MotionRotationLimits::Constrain(static_cast<MotionBodyPart>(selectedBoneIndex), previous, requested, bind);
	SetMotionRotationQuaternionKey(draft, boneName, keyFrame, constrained);
	//UI表示用にオイラーに変換する。imguiでのみ表示
	rotationEulerDegrees =
		ConvertInternalRotationToEditorRotation(selectedBoneIndex, QuaternionToEulerDegrees(constrained));

	return "Set MotionData keyframe.";
}

/// <summary>ギズモ操作で得た内部ローカル回転を、既存の数値入力と同じルールで現在フレームへ保存する。</summary>
/// <param name="bodyPartIndex">編集された部位番号。</param>
/// <param name="axisIndex">操作された軸番号。X=0, Y=1, Z=2。範囲外なら保存しない。</param>
/// <param name="keyFrame">更新対象の内部 actionFrame。</param>
/// <param name="totalFrames">MotionData の総フレーム数。</param>
/// <param name="localRotation">保存するローカル Quaternion。</param>
/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
std::string CustomizeMotionEditorController::ApplyGizmoRotationEdit(int bodyPartIndex, int axisIndex, int keyFrame,
																	int totalFrames, const Quaternion &localRotation)
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

	//Gizumoの編集結果を内部値に反映
	const std::string boneName = MotionSkeleton::GetBodyPartName(selectedBoneIndex);
	draft.totalFrames = std::max(1, totalFrames);
	SetMotionRotationQuaternionKey(draft, boneName, keyFrame, localRotation);

	rotationEulerDegrees =
		ConvertInternalRotationToEditorRotation(selectedBoneIndex, QuaternionToEulerDegrees(localRotation));
	return "Set MotionData keyframe.";
}

/// <summary>現在フレームに姿勢キーフレームがあるか確認する。</summary>
/// <param name="keyFrame">確認対象の内部 actionFrame。</param>
/// <returns>少なくとも 1 部位のキーがあれば true。</returns>
bool CustomizeMotionEditorController::HasMotionKeyframe(int keyFrame) const
{
	if (!hasDraft || keyFrame < 0)
	{
		return false;
	}

	for (const MotionBoneTrackData &track : draft.boneTracks)
	{
		if (FindMotionKeyframe(track, keyFrame))
		{
			return true;
		}
	}

	return false;
}

/// <summary>現在フレームに汎用モーション用の見た目オフセットキーを追加する。</summary>
/// <param name="keyFrame">追加先の内部 actionFrame。</param>
/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
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

/// <summary>現在フレームの汎用モーション用見た目オフセットキーを削除する。</summary>
/// <param name="keyFrame">削除対象の内部 actionFrame。</param>
/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
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

	draft.rootOffsetKeys.erase(std::remove_if(draft.rootOffsetKeys.begin(), draft.rootOffsetKeys.end(),
											  [keyFrame](const MotionRootOffsetKeyData &keyframe)
											  {
												  return keyframe.frame == keyFrame;
											  }),
							   draft.rootOffsetKeys.end());
	rootOffsetKey = GetMotionRootOffsetAtFrame(draft, keyFrame);

	return "Deleted common motion visual offset keyframe.";
}

/// <summary>現在フレームの汎用モーション用見た目オフセットキーを編集値で更新する。</summary>
/// <param name="keyFrame">更新対象の内部 actionFrame。</param>
/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
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

/// <summary>現在フレームに汎用モーション用見た目オフセットキーがあるか確認する。</summary>
/// <param name="keyFrame">確認対象の内部 actionFrame。</param>
/// <returns>rootOffsetKeys にキーがあれば true。</returns>
bool CustomizeMotionEditorController::HasRootOffsetKeyframe(int keyFrame) const
{
	if (!hasDraft || keyFrame < 0)
	{
		return false;
	}

	return FindMotionRootOffsetKeyframe(draft, keyFrame) != nullptr;
}

/// <summary>現在フレームの全身姿勢をコピーする。</summary>
/// <param name="keyFrame">コピー元の内部 actionFrame。</param>
/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
std::string CustomizeMotionEditorController::CopyWholeBodyPose(int keyFrame)
{
	if (!HasMotionKeyframe(keyFrame))
	{
		return "Copy requires a keyframe on the current frame.";
	}

	//全身分コピーする
	//選択部位単位でもできるようにする
	for (int boneIndex = 0; boneIndex < MotionEditorBoneCount; ++boneIndex)
	{
		const std::string boneName = MotionSkeleton::GetBodyPartName(boneIndex);
		copiedPoseRotations[boneIndex] = GetMotionRotationQuaternionAtFrame(draft, boneName, keyFrame);
	}

	hasCopiedPose = true;

	return "Copied whole body pose.";
}

/// <summary>コピー済みの回転を指定された部位に貼り付ける。</summary>
/// <param name="keyFrame">貼り付け先の内部フレーム。
/// 貼り付け先の内部 actionFrame。</param>
/// <param name="applyMask">適用する部位だけtrue。</param>
/// <returns>適用結果の表示メッセージ。
/// ユーザー表示用の処理結果メッセージ。</returns>
std::string CustomizeMotionEditorController::PasteCopiedPose(int keyFrame,
	const std::array<bool, MotionEditorBoneCount>& applyMask)
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
		if (!applyMask[boneIndex])
		{
			continue;
		}

		const std::string boneName = MotionSkeleton::GetBodyPartName(boneIndex);
		SetMotionRotationQuaternionKey(draft, boneName, keyFrame, copiedPoseRotations[boneIndex]);
	}

	RefreshFrameEditValues(keyFrame);

	return "ペーストしました。";
}

/// <summary>指定部位の回転をモデル座標で左右反転し、反対側のキーへ保存する。</summary>
/// <param name="preview">実ボーンの基準回転を取得するプレビュー。
/// モデルの基準回転を取得するプレビュー。</param>
/// <param name="frame">適用先の内部フレーム。</param>
/// <param name="rightToLeft">右側を元にする場合true。
/// 右から左ならtrue。</param>
/// <param name="parts">肩・肘・手首・股関節・膝・足首の適用マスク。</param>
/// <returns>適用結果の表示メッセージ。</returns>
/// <remarks>現在キーの指定部位を反転し、反対側へコピーする。</remarks>
std::string CustomizeMotionEditorController::MirrorPose(const CustomizePreviewController& preview, int frame,
	bool rightToLeft, const std::array<bool, 6>& parts)
{
	if (!HasMotionKeyframe(frame))
	{
		return "編集するフレームにポーズキーを追加してください。";
	}

	using Part = MotionBodyPart;
	constexpr Part pairs[][2] = {
		{Part::RShoulder, Part::LShoulder}, {Part::RElbow, Part::LElbow}, {Part::RHand, Part::LHand},
		{Part::RHipjoint, Part::LHipjoint}, {Part::RKnees, Part::LKnees}, {Part::RFeet, Part::LFeet}
	};
	const Matrix reflection = Matrix::CreateScale(-1.0f, 1.0f, 1.0f);
	std::array<Quaternion, 6> rotations;
	std::array<int, 6> targets{};

	for (size_t index = 0; index < parts.size(); ++index)
	{
		if (!parts[index])
		{
			continue;
		}

		const int source = static_cast<int>(pairs[index][rightToLeft ? 0 : 1]);
		const int target = static_cast<int>(pairs[index][rightToLeft ? 1 : 0]);
		Quaternion sourceBind;
		Quaternion targetBind;
		Matrix sourceParent;
		Matrix targetParent;
		if (!preview.GetBodyPartBindRotation(source, sourceBind) || !preview.GetBodyPartBindRotation(target, targetBind)
			|| !preview.GetBodyPartBindParentBasis(source, sourceParent)
			|| !preview.GetBodyPartBindParentBasis(target, targetParent))
		{
			return "左右のボーンを取得できませんでした。";
		}

		// 親の基準軸を使うため、選択外の親の現在姿勢には依存しない。
		const Quaternion current = GetMotionRotationQuaternionAtFrame(draft, MotionSkeleton::GetBodyPartName(source), frame);
		const Matrix delta = Matrix::CreateFromQuaternion(sourceBind).Transpose() * Matrix::CreateFromQuaternion(current);
		const Matrix modelDelta = sourceParent.Transpose() * delta * sourceParent;
		const Matrix reflected = reflection * modelDelta * reflection;
		const Matrix local = Matrix::CreateFromQuaternion(targetBind) * targetParent * reflected * targetParent.Transpose();
		rotations[index] = Quaternion::CreateFromRotationMatrix(local);
		rotations[index].Normalize();
		targets[index] = target;
	}

	for (size_t index = 0; index < parts.size(); ++index)
	{
		if (parts[index])
		{
			SetMotionRotationQuaternionKey(draft, MotionSkeleton::GetBodyPartName(targets[index]), frame, rotations[index]);
		}
	}

	RefreshFrameEditValues(frame);
	return "反対側へポーズをコピーしました。";
}

/// <summary>選択中 IK 対象関節のワールド位置を、現在のプレビュー姿勢から一時ターゲット入力へ読み込む。</summary>
/// <param name="previewController">現在表示中のプレビュー姿勢を持つ Controller。</param>
/// <param name="keyFrame">読み込んだターゲットが属する内部 actionFrame。</param>
/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
std::string CustomizeMotionEditorController::CaptureCurrentIkTarget(CustomizePreviewController &previewController,
																	int keyFrame)
{
	//IK対象かどうかを判定
	hasIkTargetWorldPosition = MotionIk::FindChain(static_cast<MotionBodyPart>(selectedBoneIndex)) &&
							   previewController.HasRenderedActionFrame(keyFrame) &&
							   previewController.GetBodyPartWorldPosition(selectedBoneIndex, ikTargetWorldPosition);
	//対象のキーフレームを保存
	ikTargetKeyFrame = hasIkTargetWorldPosition ? keyFrame : -1;
	//対象部位を保存
	ikTargetBodyPartIndex = hasIkTargetWorldPosition ? selectedBoneIndex : -1;

	return hasIkTargetWorldPosition ? "IK position synchronized." : "Waiting for preview pose.";
}

/// <summary>選択中 IK 対象を解き、必要な親側関節の回転だけを現在フレームの MotionData キーへ保存する。</summary>
/// <param name="previewController">現在表示中のプレビュー姿勢と IK 計算に使う Controller。</param>
/// <param name="keyFrame">保存先の内部 actionFrame。</param>
/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
std::string CustomizeMotionEditorController::ApplySelectedIkTarget(CustomizePreviewController &previewController,
																   int keyFrame)
{
	//IK未対応 or キーフレームがない場合
	if (!HasMotionKeyframe(keyFrame) || !hasIkTargetWorldPosition)
	{
		return "IK requires a pose keyframe.";
	}

	//対象部位を取得
	const auto *chain = MotionIk::FindChain(static_cast<MotionBodyPart>(selectedBoneIndex));
	MotionIkResult result;

	//IKできない場合
	if (!chain || !previewController.SolveIkTarget(selectedBoneIndex, ikTargetWorldPosition, result))
	{
		return "IK could not verify the pose. The existing keyframe was kept.";
	}
	if (result.translationLocked)
	{
		return "IK translation is locked by the fixed hierarchy.";
	}

	// 親2点の回転を変更し、手足自身のローカル回転は維持する。
	// 実FKで位置解との一致が確認された場合にだけ下書きを変更する。
	for (int i = 0; i < 3; ++i)
	{
		SetMotionRotationQuaternionKey(draft, MotionSkeleton::GetBodyPartName(static_cast<int>(chain->parts[i])),
									   keyFrame, result.localRotations[i]);
	}

	RefreshFrameEditValues(keyFrame);

	if (result.atJointLimit)
	{
		return "IK: joint rotation limit reached on the selected axis.";
	}

	return result.atReachLimit ? "IK: reached the limit on the selected axis." : "IK pose updated.";
}

/// <summary>現在姿勢を指定名のプリセットとして保存する。UIの入力バッファには依存しない。</summary>
/// <param name="keyFrame">保存元の内部フレーム。</param>
/// <param name="displayName">重複しないプリセット名。</param>
/// <returns>保存結果。</returns>
std::string CustomizeMotionEditorController::SavePosePreset(int keyFrame, const std::string &displayName)
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

/// <summary>現在フレームへ指定プリセットの選択部位だけを適用する。</summary>
/// <param name="presetId">読み込む姿勢プリセット ID。</param>
/// <param name="keyFrame">適用先の内部 actionFrame。</param>
/// <param name="applyMask">適用する部位だけ true の配列。</param>
/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
std::string CustomizeMotionEditorController::ApplyPosePreset(const std::string &presetId, int keyFrame,
															 const std::array<bool, MotionEditorBoneCount> &applyMask)
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

	return appliedCount > 0 ? "Applied pose preset: " + preset.displayName
							: "Pose preset has no matching selected parts.";
}

/// <summary>MotionData 下書きを取得する。</summary>
/// <returns>編集可能な MotionData 参照。</returns>
MotionData &CustomizeMotionEditorController::GetDraft()
{
	return draft;
}

/// <summary>MotionData 下書きを取得する。</summary>
/// <returns>読み取り専用の MotionData 参照。</returns>
const MotionData &CustomizeMotionEditorController::GetDraft() const
{
	return draft;
}

/// <summary>MotionData 下書きが有効か取得する。</summary>
/// <returns>読み込みまたは新規作成済みなら true。</returns>
bool CustomizeMotionEditorController::HasDraft() const
{
	return hasDraft;
}

/// <summary>表示名入力用バッファを取得する。</summary>
/// <returns>UI の文字入力に渡す UTF-8 固定長バッファ。</returns>
std::array<char, CustomizeMotionEditorController::MotionNameBufferSize> &CustomizeMotionEditorController::
	GetDisplayNameBuffer()
{
	return displayNameBuffer;
}

/// <summary>選択中の編集部位番号を取得する。</summary>
/// <returns>編集可能な部位番号参照。</returns>
int &CustomizeMotionEditorController::GetSelectedBoneIndex()
{
	return selectedBoneIndex;
}

/// <summary>選択部位の回転編集値を取得する。</summary>
/// <returns>編集可能なオイラー角参照。</returns>
Vector3 &CustomizeMotionEditorController::GetRotationEulerDegrees()
{
	return rotationEulerDegrees;
}

/// <summary>汎用モーション用の見た目オフセット編集値を取得する。</summary>
/// <returns>編集可能なオフセット参照。</returns>
Vector3 &CustomizeMotionEditorController::GetRootOffsetKey()
{
	return rootOffsetKey;
}

/// <summary>コピー済み姿勢が存在するか取得する。</summary>
/// <returns>Copy Pose 済みなら true。</returns>
bool CustomizeMotionEditorController::HasCopiedPose() const
{
	return hasCopiedPose;
}

/// <summary>保存前に MotionData のフレーム範囲とキー順を整える。</summary>
void CustomizeMotionEditorController::NormalizeDraftForSave()
{
	const int lastFrame = std::max(0, draft.totalFrames - 1);

	for (MotionBoneTrackData &track : draft.boneTracks)
	{
		for (MotionBoneKeyframeData &keyframe : track.keyframes)
		{
			keyframe.frame = std::clamp(keyframe.frame, 0, lastFrame);
		}

		std::sort(track.keyframes.begin(), track.keyframes.end(),
				  [](const MotionBoneKeyframeData &left, const MotionBoneKeyframeData &right)
				  {
					  return left.frame < right.frame;
				  });
	}

	draft.boneTracks.erase(std::remove_if(draft.boneTracks.begin(), draft.boneTracks.end(),
										  [](const MotionBoneTrackData &track)
										  {
											  return track.keyframes.empty();
										  }),
						   draft.boneTracks.end());

	for (MotionRootOffsetKeyData &keyframe : draft.rootOffsetKeys)
	{
		keyframe.frame = std::clamp(keyframe.frame, 0, lastFrame);
	}

	std::sort(draft.rootOffsetKeys.begin(), draft.rootOffsetKeys.end(),
			  [](const MotionRootOffsetKeyData &left, const MotionRootOffsetKeyData &right)
			  {
				  return left.frame < right.frame;
			  });
}
