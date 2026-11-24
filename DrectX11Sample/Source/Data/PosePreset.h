#pragma once

#include <SimpleMath.h>

#include <string>
#include <vector>

/// <summary>
/// 1 部位分のプリセット姿勢回転を保持する。
/// </summary>
struct PosePresetBoneData
{
	// MotionSkeletonDefinition 側の編集用部位名。
	std::string boneName;

	// 保存・適用時に正とするローカル Quaternion 回転。
	DirectX::SimpleMath::Quaternion localRotation = DirectX::SimpleMath::Quaternion::Identity;

	// 旧 rotationEulerDegrees 読み込み互換と UI 表示確認用の Euler 回転。
	DirectX::SimpleMath::Vector3 localRotationEulerDegrees = DirectX::SimpleMath::Vector3::Zero;
};

/// <summary>
/// 1 つの全身姿勢プリセットを保持する。
/// </summary>
struct PosePresetData
{
	std::string presetId;
	std::string displayName;
	std::vector<PosePresetBoneData> bones;
};

class PosePresetStore
{
public:
	static std::vector<std::string> ListPresetIds();

	static bool LoadPreset(const std::string& presetId, PosePresetData& outPreset);

	static bool SavePreset(const std::string& presetId, const PosePresetData& preset);

	static std::string CreateUniquePresetId();

	static bool DisplayNameExists(const std::string& displayName);

	static bool Exists(const std::string& presetId);

	static bool IsValidPresetId(const std::string& presetId);
};
