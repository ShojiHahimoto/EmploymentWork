#pragma once

#include "Component/SkeletonPoseComponent.h"
#include "Resource/ModelResource.h"

#include <SimpleMath.h>

#include <string>

class World;

/// <summary>
/// ModelResource のボーン情報と GameObject ごとの姿勢 Component から、描画用スキニング行列を作る。
/// </summary>
class MotionSystem
{
public:
	static void Update(World& world);

	/// <summary>
	/// SkeletonPoseComponent を ModelResource の bind pose で初期化する。
	/// </summary>
	/// <param name="pose">初期化する GameObject ごとの姿勢 Component。</param>
	/// <param name="model">初期ボーン姿勢を持つ ModelResource。</param>
	/// <param name="modelKey">初期化元として記録する ModelResource キー。</param>
	/// <returns>初期化できた場合は true。</returns>
	static bool InitializeSkeletonPose(
		SkeletonPoseComponent& pose,
		const ModelResource& model,
		const std::string& modelKey);

	/// <summary>
	/// 指定ボーンのローカル回転を Euler 角 degree で設定する。
	/// </summary>
	/// <param name="pose">変更する GameObject ごとの姿勢 Component。</param>
	/// <param name="model">ボーン名検索に使う ModelResource。</param>
	/// <param name="boneName">変更するボーン名。</param>
	/// <param name="eulerDegrees">設定するローカル Euler 回転。</param>
	/// <returns>対象ボーンが見つかり設定できた場合は true。</returns>
	static bool SetBoneLocalEulerRotationDegrees(
		SkeletonPoseComponent& pose,
		const ModelResource& model,
		const std::string& boneName,
		const DirectX::SimpleMath::Vector3& eulerDegrees);

	/// <summary>
	/// 現在のローカルボーン姿勢から、親子階層反映済みのスキニング行列を更新する。
	/// </summary>
	/// <param name="pose">計算結果を書き込む姿勢 Component。</param>
	/// <param name="model">ボーン階層と offsetMatrix を持つ ModelResource。</param>
	static void UpdateSkinningMatrices(SkeletonPoseComponent& pose, const ModelResource& model);

private:
	static void ResetPoseToBindPose(SkeletonPoseComponent& pose, const ModelResource& model);
	static void ApplyDebugPose(SkeletonPoseComponent& pose, const ModelResource& model);
	static DirectX::SimpleMath::Matrix CreateLocalMatrix(const BonePose& pose);
};
