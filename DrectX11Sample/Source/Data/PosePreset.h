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

	// MotionData と同じ内部保存値の Euler 回転。
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
	/// <summary>
	/// 保存済みプリセット ID の一覧を assets/PosePreset から取得する。
	/// </summary>
	/// <returns>拡張子なしのプリセット ID 一覧。</returns>
	static std::vector<std::string> ListPresetIds();

	/// <summary>
	/// 指定 ID の姿勢プリセットを読み込む。
	/// </summary>
	/// <param name="presetId">assets/PosePreset 配下の拡張子なし ID。</param>
	/// <param name="outPreset">読み込んだプリセットの書き込み先。</param>
	/// <returns>読み込みに成功した場合は true。</returns>
	static bool LoadPreset(const std::string& presetId, PosePresetData& outPreset);

	/// <summary>
	/// 姿勢プリセットを assets/PosePreset 配下へ保存する。
	/// </summary>
	/// <param name="presetId">保存先の拡張子なし ID。</param>
	/// <param name="preset">保存するプリセット。</param>
	/// <returns>保存に成功した場合は true。</returns>
	static bool SavePreset(const std::string& presetId, const PosePresetData& preset);

	/// <summary>
	/// 指定 ID のプリセットファイルが既に存在するか確認する。
	/// </summary>
	/// <param name="presetId">確認する拡張子なし ID。</param>
	/// <returns>同名プリセットが存在する場合は true。</returns>
	static bool Exists(const std::string& presetId);

	/// <summary>
	/// ファイル名として危険な文字を含まないプリセット ID か確認する。
	/// </summary>
	/// <param name="presetId">確認するプリセット ID。</param>
	/// <returns>保存 ID として使える場合は true。</returns>
	static bool IsValidPresetId(const std::string& presetId);
};
