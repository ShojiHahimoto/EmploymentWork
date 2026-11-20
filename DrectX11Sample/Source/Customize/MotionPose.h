#pragma once

#include "Data/SkeletonPose.h"
#include "Data/MotionData.h"
#include "Data/AttackData.h"
#include "Resource/ModelResource.h"

/// <summary>
/// 編集と対戦で共有する姿勢計算。状態遷移や再生時計は変更しない。
/// </summary>
class MotionPose
{
public:
	/// <summary>ボーンのモデル空間行列を実ワールド空間へ変換する。</summary>
	/// <param name="pose">階層計算済みの姿勢。</param>
	/// <param name="boneIndex">実ボーン番号。</param>
	/// <param name="objectWorld">モデル本体のワールド行列。</param>
	/// <param name="worldMatrix">取得結果。失敗時は変更しない。</param>
	/// <returns>有効なボーンなら true。</returns>
	static bool GetBoneWorldMatrix(const SkeletonPose& pose, int boneIndex,
		const DirectX::SimpleMath::Matrix& objectWorld, DirectX::SimpleMath::Matrix& worldMatrix);

	/// <summary>実ワールド行列を親基準のローカル姿勢へ変換する。計算だけを行う。</summary>
	/// <param name="pose">階層計算済みの姿勢。</param>
	/// <param name="model">実際の親ボーン階層。</param>
	/// <param name="boneIndex">変換する実ボーン番号。</param>
	/// <param name="objectWorld">モデル本体のワールド行列。</param>
	/// <param name="worldMatrix">変換する実ワールド行列。</param>
	/// <param name="localPose">変換結果。失敗時は変更しない。</param>
	/// <returns>逆行列と姿勢分解が成立した場合は true。</returns>
	static bool WorldToLocalPose(const SkeletonPose& pose, const ModelResource& model, int boneIndex,
		const DirectX::SimpleMath::Matrix& objectWorld, const DirectX::SimpleMath::Matrix& worldMatrix,
		BonePose& localPose);

	/// <summary>
	/// SkeletonPose を ModelResource の bind pose で初期化する。
	/// </summary>
	/// <param name="pose">初期化する GameObject ごとの姿勢 Component。</param>
	/// <param name="model">初期ボーン姿勢を持つ ModelResource。</param>
	/// <param name="modelKey">初期化元として記録する ModelResource キー。</param>
	/// <returns>初期化できた場合は true。</returns>
	static bool InitializeSkeletonPose(
		SkeletonPose& pose,
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
		SkeletonPose& pose,
		const ModelResource& model,
		const std::string& boneName,
		const DirectX::SimpleMath::Vector3& eulerDegrees);

	/// <summary>
	/// 現在のローカルボーン姿勢から、親子階層反映済みのスキニング行列を更新する。
	/// </summary>
	/// <param name="pose">計算結果を書き込む姿勢 Component。</param>
	/// <param name="model">ボーン階層と offsetMatrix を持つ ModelResource。</param>
	static void UpdateSkinningMatrices(SkeletonPose& pose, const ModelResource& model);

	/// <summary>
	/// 指定 MotionData の指定フレームを SkeletonPose に反映する。
	/// </summary>
	/// <param name="pose">変更する姿勢 Component。</param>
	/// <param name="motion">適用するモーションデータ。</param>
	/// <param name="frame">再生する 0 始まりフレーム。</param>
	/// <param name="model">ボーン名検索と bind pose 取得に使う ModelResource。</param>
	static void ApplyMotionData(
		SkeletonPose& pose,
		const MotionData& motion,
		int frame,
		const ModelResource& model);

	/// <summary>
	/// 指定MotionDataを下地姿勢から補間し、指定時は末尾から下地へ戻す。
	/// </summary>
	/// <param name="pose">変更する姿勢 Component。</param>
	/// <param name="motion">適用するモーションデータ。</param>
	/// <param name="frame">再生する 0 始まりフレーム。</param>
	/// <param name="model">ボーン名検索と bind pose 取得に使う ModelResource。</param>
	/// <param name="basePose">開始・終了と未指定ボーンに使う基準姿勢。nullptrならbind pose。</param>
	/// <param name="returnToBase">非ループの末尾キー以降を最終フレームまでに基準姿勢へ戻す場合true。端点の実キーを優先し、ループ再生では無視する。</param>
	static void ApplyMotionData(
		SkeletonPose& pose,
		const MotionData& motion,
		int frame,
		const ModelResource& model,
		const SkeletonPose* basePose, bool returnToBase = false);

	/// <summary>技の発動条件に対応する端点姿勢のモーションIDを返す。</summary>
	/// <param name="usableState">技の地上・空中発動条件。</param>
	/// <returns>空中はCommon/JumpLoop、それ以外はCommon/Idle。基準姿勢には0Fを使う。</returns>
	static const char* GetAttackBaseMotionId(AttackUsableState usableState);

	/// <summary>ローカル姿勢を bind pose へ戻す。行列は別途更新する。</summary>
	/// <param name="pose">リセットする姿勢。</param>
	/// <param name="model">初期姿勢を持つモデル。</param>
	static void ResetPoseToBindPose(SkeletonPose& pose, const ModelResource& model);

	/// <summary>共通サンプリング。returnToBaseは攻撃の末尾から基準姿勢へ戻す場合に指定する。</summary>
	/// <param name="track">時系列順のボーンキー。</param>
	/// <param name="bindPose">未指定成分と前後の補間に使う基準姿勢。モデルのbind poseに限らない。</param>
	/// <param name="frame">内部0始まりの取得フレーム。非ループの開始基準は-1。</param>
	/// <param name="totalFrames">総フレーム数。非ループの終了基準はtotalFrames - 1。</param>
	/// <param name="looping">末尾と先頭をループ補間する場合true。</param>
	/// <param name="returnToBase">非ループの末尾キー以降を基準姿勢へ補間する場合true。falseなら末尾姿勢を維持する。</param>
	/// <returns>補間済みローカル姿勢。キーなしなら基準姿勢。</returns>
	static BonePose SampleBoneTrack(
		const MotionBoneTrackData& track,
		const BonePose& bindPose,
		int frame,
		int totalFrames,
		bool looping, bool returnToBase = false);

private:
	static DirectX::SimpleMath::Matrix CreateLocalMatrix(const BonePose& pose);
};
