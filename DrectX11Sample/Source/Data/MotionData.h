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
/// assets/MotionData 配下の JSON 1 つに対応する自作モーションデータ。
/// </summary>
struct MotionData
{
	std::string motionDataId;
	std::string displayName;
	int totalFrames = 1;
	bool looping = false;
	std::vector<MotionBoneTrackData> boneTracks;
};

class MotionDataManager
{
public:
	/// <summary>
	/// 指定 ID の MotionData を読み込み、読み込み済みなら再利用する。
	/// </summary>
	/// <param name="motionDataId">assets/MotionData 配下のモーション ID。</param>
	/// <returns>読み込みまたは再利用に成功した場合は true。</returns>
	static bool LoadMotionData(const std::string& motionDataId);

	/// <summary>
	/// 読み込み済み MotionData を取得する。
	/// </summary>
	/// <param name="motionDataId">取得するモーション ID。</param>
	/// <returns>見つかった MotionData。存在しない場合は nullptr。</returns>
	static const MotionData* GetMotionData(const std::string& motionDataId);

	/// <summary>
	/// 読み込み済み MotionData を全て破棄する。
	/// </summary>
	static void UnloadAll();

private:
	static std::unordered_map<std::string, std::unique_ptr<MotionData>> resources;
};
