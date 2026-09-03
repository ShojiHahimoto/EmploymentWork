#pragma once

#include "Component/Component.h"

#include <SimpleMath.h>

#include <string>
#include <vector>

/// <summary>
/// 1 ボーン分のローカル姿勢を保持する。
/// </summary>
struct BonePose
{
	DirectX::SimpleMath::Vector3 localPosition = DirectX::SimpleMath::Vector3::Zero;
	DirectX::SimpleMath::Quaternion localRotation = DirectX::SimpleMath::Quaternion::Identity;
	DirectX::SimpleMath::Vector3 localScale = DirectX::SimpleMath::Vector3::One;
};

/// <summary>
/// ModelResource を使う GameObject ごとの現在スケルトン姿勢を保持する。
/// </summary>
struct SkeletonPoseComponent : public Component
{
	// どの ModelResource から初期姿勢を作ったかを保持し、モデル差し替え時に初期化し直す。
	std::string sourceModelKey;

	// 各ボーンの現在ローカル姿勢。MotionSystem が ModelResource の bind pose から初期化する。
	std::vector<BonePose> bonePoses;

	// 親子階層を反映した各ボーンの現在ワールド行列。
	std::vector<DirectX::SimpleMath::Matrix> boneWorldMatrices;

	// Renderer に渡す最終スキニング行列。頂点の boneIndices / boneWeights から参照される。
	std::vector<DirectX::SimpleMath::Matrix> skinningMatrices;

	// 初期化済みなら true。ModelComponent の resourceKey が変わった場合は false に戻す。
	bool initialized = false;

	// スキニング確認用の仮ポーズを有効にする。将来の MotionData 再生が入ったら削除または Debug 専用に畳む。
	bool enableDebugPose = false;

	// Debug ポーズをフレームベースで動かすためのカウンタ。
	int debugPoseFrame = 0;
};
