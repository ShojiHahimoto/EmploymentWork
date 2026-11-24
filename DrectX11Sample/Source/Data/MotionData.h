#pragma once

#include <SimpleMath.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

/// <summary>
/// 1 ボーンの指定フレームにおけるローカル姿勢キーを保持する。
/// </summary>
struct MotionBoneKeyframeData
{
	// モーション開始を 0 としたキーフレーム番号。
	int frame = 0;

	// true の場合だけ localPosition を現在姿勢へ反映する。
	bool hasPosition = false;
	DirectX::SimpleMath::Vector3 localPosition = DirectX::SimpleMath::Vector3::Zero;

	// true の場合だけ localRotation を現在姿勢へ反映する。
	bool hasRotation = false;
	DirectX::SimpleMath::Vector3 localRotationEulerDegrees = DirectX::SimpleMath::Vector3::Zero;
	DirectX::SimpleMath::Quaternion localRotation = DirectX::SimpleMath::Quaternion::Identity;

	// true の場合だけ localScale を現在姿勢へ反映する。
	bool hasScale = false;
	DirectX::SimpleMath::Vector3 localScale = DirectX::SimpleMath::Vector3::One;
};

/// <summary>
/// 1 ボーン分のキーフレーム配列を保持する。
/// </summary>
struct MotionBoneTrackData
{
	// ModelResource 側のボーン名と対応する。
	std::string boneName;
	std::vector<MotionBoneKeyframeData> keyframes;
};

/// <summary>
/// 汎用モーションの見た目だけをずらす全身オフセットキーを保持する。
/// </summary>
struct MotionRootOffsetKeyData
{
	// モーション開始を 0 としたキーフレーム番号。
	int frame = 0;

	// プレイヤー本体座標ではなく、モデル描画だけに加える見た目用オフセット。
	DirectX::SimpleMath::Vector3 offset = DirectX::SimpleMath::Vector3::Zero;
};

/// <summary>
/// assets/MotionData 配下の JSON 1 つに対応する自作モーションデータ。
/// </summary>
struct MotionData
{
	std::string motionDataId;
	std::string displayName;
	int totalFrames = 1;
	bool looping = false;
	std::vector<MotionBoneTrackData> boneTracks;
	std::vector<MotionRootOffsetKeyData> rootOffsetKeys;
};

class MotionDataManager
{
public:
	static bool LoadMotionData(const std::string& motionDataId);

	static const MotionData* GetMotionData(const std::string& motionDataId);

	static void UnloadAll();

private:
	static std::unordered_map<std::string, std::unique_ptr<MotionData>> resources;
};
