#pragma once

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
/// 所有者から独立した現在スケルトン姿勢。編集画面でも Component なしで保持できる。
/// </summary>
struct SkeletonPose
{
	// どの ModelResource から初期姿勢を作ったかを保持し、モデル差し替え時に初期化し直す。
	std::string sourceModelKey;

	// 実親ボーン基準のローカル姿勢。MotionPose がモデルの bind pose から初期化する。
	std::vector<BonePose> bonePoses;

	// 既存の名前を維持しているが、実体は階層反映済みのモデル空間行列。本体 Transform は含まない。
	std::vector<DirectX::SimpleMath::Matrix> boneWorldMatrices;

	// Renderer に渡す最終スキニング行列。頂点の boneIndices / boneWeights から参照される。
	std::vector<DirectX::SimpleMath::Matrix> skinningMatrices;

	// 初期化済みなら true。ModelComponent の resourceKey が変わった場合は false に戻す。
	bool initialized = false;

};
