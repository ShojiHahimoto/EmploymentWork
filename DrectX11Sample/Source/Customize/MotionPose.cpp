#include "Customize/MotionPose.h"
#include "Data/MotionSkeletonDefinition.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>

using namespace DirectX;
using namespace DirectX::SimpleMath;

/// <summary>計算済みボーン姿勢にモデル本体の変換を適用する。</summary>
/// <param name="pose">階層計算済み姿勢。</param>
/// <param name="boneIndex">実ボーン番号。</param>
/// <param name="objectWorld">本体の変換。</param>
/// <param name="worldMatrix">結果の出力先。</param>
/// <returns>取得できれば true。</returns>
bool MotionPose::GetBoneWorldMatrix(const SkeletonPose& pose, int boneIndex,
	const Matrix& objectWorld, Matrix& worldMatrix)
{
	if (boneIndex < 0 || static_cast<size_t>(boneIndex) >= pose.boneModelMatrices.size())
		return false;
	worldMatrix = pose.boneModelMatrices[boneIndex] * objectWorld;
	return true;
}

/// <summary>親の実ワールド変換を取り除き、ローカル姿勢を求める。</summary>
/// <param name="pose">階層計算済み姿勢。</param>
/// <param name="model">実ボーン階層。</param>
/// <param name="boneIndex">対象番号。</param>
/// <param name="objectWorld">本体の変換。</param>
/// <param name="worldMatrix">変換元。</param>
/// <param name="localPose">成功時の出力先。</param>
/// <returns>変換できれば true。</returns>
bool MotionPose::WorldToLocalPose(const SkeletonPose& pose, const ModelResource& model,
	int boneIndex, const Matrix& objectWorld, const Matrix& worldMatrix, BonePose& localPose)
{
	//ボーンを取得
	const auto& bones = model.GetBones();
	//取得できばければfalse
	if (boneIndex < 0 || static_cast<size_t>(boneIndex) >= bones.size())
		return false;
	//親のワールド行列を取得
	Matrix parentWorld = objectWorld;
	const int parent = bones[boneIndex].parentIndex;
	//取得したのボーンが親 or ボーンが取得できなければfalse
	if (parent >= 0 && !GetBoneWorldMatrix(pose, parent, objectWorld, parentWorld))
		return false;
	const float determinant = parentWorld.Determinant();
	if (!std::isfinite(determinant) || std::abs(determinant) < 1e-12f)
		return false;
	//親の逆行列をかけて、ローカル行列に変換
	Matrix local = worldMatrix * parentWorld.Invert();
	BonePose result;
	if (!local.Decompose(result.localScale, result.localRotation, result.localPosition))
		return false;
	result.localRotation.Normalize();
	localPose = result;
	return true;
}

/// <summary>
/// ModelResource の bind pose を GameObject ごとの現在姿勢へコピーする。
/// </summary>
/// <param name="pose">初期化する姿勢 Component。</param>
/// <param name="model">初期ボーン姿勢を持つ ModelResource。</param>
/// <param name="modelKey">初期化元として記録するモデルキー。</param>
/// <returns>ボーンが存在し、初期化できた場合は true。</returns>
bool MotionPose::InitializeSkeletonPose(
	SkeletonPose& pose,
	const ModelResource& model,
	const std::string& modelKey)
{
	const std::vector<ModelBone>& bones = model.GetBones();
	if (bones.empty())
	{
		pose.sourceModelKey.clear();
		pose.bonePoses.clear();
		pose.boneModelMatrices.clear();
		pose.skinningMatrices.clear();
		pose.initialized = false;
		return false;
	}

	pose.sourceModelKey = modelKey;
	pose.bonePoses.resize(bones.size());
	pose.boneModelMatrices.assign(bones.size(), Matrix::Identity);
	pose.skinningMatrices.assign(bones.size(), Matrix::Identity);
	ResetPoseToBindPose(pose, model);
	UpdateSkinningMatrices(pose, model);
	pose.initialized = true;

	return true;
}

/// <summary>
/// モーション編集やデバッグ確認用に、指定ボーンのローカル回転を直接設定する。
/// </summary>
/// <param name="pose">変更する姿勢 Component。</param>
/// <param name="model">ボーン名検索に使う ModelResource。</param>
/// <param name="boneName">対象ボーン名。</param>
/// <param name="eulerDegrees">degree 単位のローカル Euler 回転。</param>
/// <returns>対象ボーンが見つかり、姿勢を変更できた場合は true。</returns>
bool MotionPose::SetBoneLocalEulerRotationDegrees(
	SkeletonPose& pose,
	const ModelResource& model,
	const std::string& boneName,
	const Vector3& eulerDegrees)
{
	const int boneIndex = model.FindBoneIndex(boneName);
	if (boneIndex < 0 || static_cast<size_t>(boneIndex) >= pose.bonePoses.size())
	{
		return false;
	}

	const float pitch = XMConvertToRadians(eulerDegrees.x);
	const float yaw = XMConvertToRadians(eulerDegrees.y);
	const float roll = XMConvertToRadians(eulerDegrees.z);
	pose.bonePoses[boneIndex].localRotation = Quaternion::CreateFromYawPitchRoll(yaw, pitch, roll);
	pose.bonePoses[boneIndex].localRotation.Normalize();

	return true;
}

/// <summary>
/// 現在ローカル姿勢から親子階層を反映した行列を作り、Renderer 用のスキニング行列を更新する。
/// </summary>
/// <param name="pose">計算結果を書き込む姿勢 Component。</param>
/// <param name="model">親子階層と offsetMatrix を持つ ModelResource。</param>
void MotionPose::UpdateSkinningMatrices(SkeletonPose& pose, const ModelResource& model)
{
	const std::vector<ModelBone>& bones = model.GetBones();
	if (bones.empty() || pose.bonePoses.size() != bones.size())
	{
		return;
	}

	pose.boneModelMatrices.assign(bones.size(), Matrix::Identity);
	pose.skinningMatrices.assign(bones.size(), Matrix::Identity);

	// ModelResource は親を先に登録する。15部位の編集用階層ではなく、全ボーンを含む実階層で FK を行う。
	// 計算結果はローカル空間。本体 Transform を含む実ワールド変換は GetBoneWorldMatrix で行う。
	for (size_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex)
	{
		const Matrix localMatrix = CreateLocalMatrix(pose.bonePoses[boneIndex]);
		const int parentIndex = bones[boneIndex].parentIndex;

		//親がいる場合親の行列をかける。親から登録しているため、親の行列にはすでにさらに上の親からの変換結果が適応されている
		if (parentIndex >= 0 && static_cast<size_t>(parentIndex) < pose.boneModelMatrices.size())
		{
			pose.boneModelMatrices[boneIndex] = localMatrix * pose.boneModelMatrices[parentIndex];
		}
		else
		{
			pose.boneModelMatrices[boneIndex] = localMatrix;
		}

		// offsetMatrix は bind pose の逆変換、boneWorldMatrices は現在姿勢。
		// 頂点は bind 空間 -> bone 空間 -> 現在姿勢の順に変換される。
		pose.skinningMatrices[boneIndex] = bones[boneIndex].offsetMatrix * pose.boneModelMatrices[boneIndex];
	}
}

/// <summary>
/// 現在姿勢をモデル読み込み時の bind pose へ戻す。
/// </summary>
/// <param name="pose">リセットする姿勢 Component。</param>
/// <param name="model">bind pose を持つ ModelResource。</param>
void MotionPose::ResetPoseToBindPose(SkeletonPose& pose, const ModelResource& model)
{
	const std::vector<ModelBone>& bones = model.GetBones();
	pose.bonePoses.resize(bones.size());

	for (size_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex)
	{
		pose.bonePoses[boneIndex].localPosition = bones[boneIndex].bindLocalPosition;
		pose.bonePoses[boneIndex].localRotation = bones[boneIndex].bindLocalRotation;
		pose.bonePoses[boneIndex].localScale = bones[boneIndex].bindLocalScale;
	}
}

/// <summary>
/// 指定 MotionData の指定フレームを SkeletonPose に反映する。
/// </summary>
/// <param name="pose">変更する姿勢 Component。</param>
/// <param name="motion">適用するモーションデータ。</param>
/// <param name="frame">再生する 0 始まりフレーム。</param>
/// <param name="model">ボーン名検索と bind pose 取得に使う ModelResource。</param>
void MotionPose::ApplyMotionData(
	SkeletonPose& pose,
	const MotionData& motion,
	int frame,
	const ModelResource& model)
{
	ApplyMotionData(pose, motion, frame, model, nullptr);
}

/// <summary>
/// 指定 MotionData を、別姿勢を下地にして SkeletonPose へ反映する。
/// </summary>
/// <param name="pose">変更する姿勢 Component。</param>
/// <param name="motion">適用するモーションデータ。</param>
/// <param name="frame">再生する 0 始まりフレーム。</param>
/// <param name="model">ボーン名検索と bind pose 取得に使う ModelResource。</param>
/// <param name="basePose">開始・終了と未指定ボーンに使う基準姿勢。nullptrならbind pose。</param>
/// <param name="returnToBase">trueの場合末尾フレーム以降は基準姿勢に向けて保管する。最終フレームにキーがある場合キーを優先する。ループ再生では無視する。</param>
void MotionPose::ApplyMotionData(
	SkeletonPose& pose,
	const MotionData& motion,
	int frame,
	const ModelResource& model,
	const SkeletonPose* basePose, bool returnToBase)
{
	ResetPoseToBindPose(pose, model);
	if (basePose && basePose->bonePoses.size() == pose.bonePoses.size())
	{
		pose.bonePoses = basePose->bonePoses;
	}

	for (const MotionBoneTrackData& track : motion.boneTracks)
	{
		const int boneIndex = MotionSkeleton::FindModelBoneIndex(model, track.boneName);
		if (boneIndex < 0 || static_cast<size_t>(boneIndex) >= pose.bonePoses.size())
		{
			continue;
		}

		const BonePose bindPose =
		{
			pose.bonePoses[boneIndex].localPosition,
			pose.bonePoses[boneIndex].localRotation,
			pose.bonePoses[boneIndex].localScale
		};

		pose.bonePoses[boneIndex] = SampleBoneTrack(track, bindPose, frame, motion.totalFrames, motion.looping, returnToBase);
	}
}

/// <summary>
/// 1 ボーントラックから指定フレームのローカル姿勢を補間して取得する。
/// </summary>
/// <param name="track">参照するボーンキーフレーム配列。</param>
/// <param name="bindPose">未指定成分と前後の補間に使う基準姿勢。モデルのbind poseに限らない。</param>
/// <param name="frame">取得する 0 始まりフレーム。</param>
/// <param name="totalFrames">総フレーム数。非ループの終了基準はtotalFrames - 1。</param>
/// <param name="looping">最後のキーから最初のキーへ補間する場合は true。</param>
/// <param name="returnToBase">非ループの末尾キー以降を基準姿勢へ補間する場合true。falseなら末尾姿勢を維持する。</param>
/// <returns>指定フレームにおける 1 ボーン分のローカル姿勢。</returns>
BonePose MotionPose::SampleBoneTrack(
	const MotionBoneTrackData& track,
	const BonePose& bindPose,
	int frame,
	int totalFrames,
	bool looping, bool returnToBase)
{
	BonePose result = bindPose;
	if (track.keyframes.empty())
	{
		return result;
	}

	const int clampedTotalFrames = std::max(1, totalFrames);
	if (looping)
	{
		frame %= clampedTotalFrames;
		if (frame < 0)
		{
			frame += clampedTotalFrames;
		}
	}

	const MotionBoneKeyframeData* previousKey = nullptr;
	const MotionBoneKeyframeData* nextKey = nullptr;
	for (const MotionBoneKeyframeData& keyframe : track.keyframes)
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

	//ループの処理
	if (looping)
	{
		if (!previousKey)
		{
			previousKey = &track.keyframes.back();
		}
		if (!nextKey)
		{
			nextKey = &track.keyframes.front();
		}

		//最後のキーより後ろのフレームは、次のループの最初のキーへ保管する
		const bool crossesLoop = previousKey->frame > nextKey->frame;
		const int frameSpan = crossesLoop
			? std::max(1, (clampedTotalFrames - previousKey->frame) + nextKey->frame)
			: std::max(1, nextKey->frame - previousKey->frame);
		const int frameOffset = crossesLoop && frame < nextKey->frame
			? (clampedTotalFrames - previousKey->frame) + frame
			: frame - previousKey->frame;
		const float t = previousKey == nextKey
			? 0.0f
			: std::clamp(static_cast<float>(frameOffset) / static_cast<float>(frameSpan), 0.0f, 1.0f);

		if (previousKey->hasPosition && nextKey->hasPosition)
		{
			result.localPosition = Vector3::Lerp(previousKey->localPosition, nextKey->localPosition, t);
		}
		else if (previousKey->hasPosition)
		{
			result.localPosition = previousKey->localPosition;
		}

		if (previousKey->hasRotation && nextKey->hasRotation)
		{
			result.localRotation = Quaternion::Slerp(previousKey->localRotation, nextKey->localRotation, t);
			result.localRotation.Normalize();
		}
		else if (previousKey->hasRotation)
		{
			result.localRotation = previousKey->localRotation;
		}

		if (previousKey->hasScale && nextKey->hasScale)
		{
			result.localScale = Vector3::Lerp(previousKey->localScale, nextKey->localScale, t);
		}
		else if (previousKey->hasScale)
		{
			result.localScale = previousKey->localScale;
		}

		return result;
	}

	//非ループの処理
	if (!previousKey)
	{
		previousKey = &track.keyframes.front();
	}
	if (!nextKey)
	{
		nextKey = &track.keyframes.back();
	}

	// 最初のキーより前は、待機姿勢から最初のキーへ補間する。
	// これにより、1F目に明示的に待機姿勢を置く必要がなくなる。
	if (frame < track.keyframes.front().frame)
	{
		const MotionBoneKeyframeData& firstKey = track.keyframes.front();
		const int frameSpan = std::max(1, firstKey.frame + 1);
		const float t = std::clamp(static_cast<float>(frame + 1) / static_cast<float>(frameSpan), 0.0f, 1.0f);

		if (firstKey.hasPosition)
		{
			result.localPosition = Vector3::Lerp(bindPose.localPosition, firstKey.localPosition, t);
		}

		if (firstKey.hasRotation)
		{
			result.localRotation = Quaternion::Slerp(bindPose.localRotation, firstKey.localRotation, t);
			result.localRotation.Normalize();
		}

		if (firstKey.hasScale)
		{
			result.localScale = Vector3::Lerp(bindPose.localScale, firstKey.localScale, t);
		}

		return result;
	}

	if (returnToBase && frame > track.keyframes.back().frame)
	{
		const auto& last = track.keyframes.back();
		const int span = std::max(1, clampedTotalFrames - 1 - last.frame);
		const float t = std::clamp(static_cast<float>(frame - last.frame) / span, 0.0f, 1.0f);

		if (last.hasPosition)
		{
			result.localPosition = Vector3::Lerp(last.localPosition, bindPose.localPosition, t);
		}

		if (last.hasRotation)
		{
			result.localRotation = Quaternion::Slerp(last.localRotation, bindPose.localRotation, t);
			result.localRotation.Normalize();
		}

		if (last.hasScale)
		{
			result.localScale = Vector3::Lerp(last.localScale, bindPose.localScale, t);
		}

		return result;
	}

	//キーフレーム間は前のキーと後ろのキーで補完する
	const int frameSpan = std::max(1, nextKey->frame - previousKey->frame);
	const float t = std::clamp(static_cast<float>(frame - previousKey->frame) / static_cast<float>(frameSpan), 0.0f, 1.0f);

	if (previousKey->hasPosition && nextKey->hasPosition)
	{
		result.localPosition = Vector3::Lerp(previousKey->localPosition, nextKey->localPosition, t);
	}
	else if (previousKey->hasPosition)
	{
		result.localPosition = previousKey->localPosition;
	}

	if (previousKey->hasRotation && nextKey->hasRotation)
	{
		result.localRotation = Quaternion::Slerp(previousKey->localRotation, nextKey->localRotation, t);
		result.localRotation.Normalize();
	}
	else if (previousKey->hasRotation)
	{
		result.localRotation = previousKey->localRotation;
	}

	if (previousKey->hasScale && nextKey->hasScale)
	{
		result.localScale = Vector3::Lerp(previousKey->localScale, nextKey->localScale, t);
	}
	else if (previousKey->hasScale)
	{
		result.localScale = previousKey->localScale;
	}

	return result;
}

/// <summary>技の発動条件から、固定の開始・終了姿勢を選ぶ。</summary>
/// <param name="usableState">技データの地上・空中条件。</param>
/// <returns>基準モーションのID。姿勢はその0Fを使用する。</returns>
const char* MotionPose::GetAttackBaseMotionId(AttackUsableState usableState)
{
	return usableState == AttackUsableState::Air ? "Common/JumpLoop" : "Common/Idle";
}

/// <summary>ローカル姿勢の拡縮・回転・移動を行列へ合成する。</summary>
/// <param name="pose">行列化する姿勢。</param>
/// <returns>ローカル変換行列。</returns>
Matrix MotionPose::CreateLocalMatrix(const BonePose& pose)
{
	return Matrix::CreateScale(pose.localScale)
		* Matrix::CreateFromQuaternion(pose.localRotation)
		* Matrix::CreateTranslation(pose.localPosition);
}
