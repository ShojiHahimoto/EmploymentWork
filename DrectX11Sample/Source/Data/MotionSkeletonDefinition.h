#pragma once

#include <array>
#include <string_view>

class ModelResource;

/// <summary>
/// モーション編集で扱う標準部位を識別する。
/// </summary>
enum class MotionBodyPart : int
{
	None = -1,
	Head = 0,
	Spine,
	Waist,
	RShoulder,
	LShoulder,
	RElbow,
	LElbow,
	RHand,
	LHand,
	RHipjoint,
	LHipjoint,
	RKnees,
	LKnees,
	RFeet,
	LFeet,
	Count
};

inline constexpr int MotionBodyPartCount = static_cast<int>(MotionBodyPart::Count);

/// <summary>
/// 標準部位とモデル側ボーンの対応、親部位を保持する。
/// </summary>
struct MotionBodyPartDefinition
{
	MotionBodyPart bodyPart = MotionBodyPart::None;
	const char* editorName = "";
	MotionBodyPart parent = MotionBodyPart::None;
	std::array<const char*, 4> modelBoneNames = {};
};

namespace MotionSkeleton
{
	/// <summary>
	/// 15部位の共通定義一覧を取得する。
	/// </summary>
	/// <returns>編集用部位の定義一覧。</returns>
	const std::array<MotionBodyPartDefinition, MotionBodyPartCount>& GetBodyPartDefinitions();

	/// <summary>
	/// 部位番号に対応する共通定義を取得する。
	/// </summary>
	/// <param name="index">0から始まる部位番号。</param>
	/// <returns>範囲内へ補正した部位定義。</returns>
	const MotionBodyPartDefinition& GetBodyPartDefinition(int index);

	/// <summary>
	/// 部位番号から編集画面用の正式名称を取得する。
	/// </summary>
	/// <param name="index">0から始まる部位番号。</param>
	/// <returns>HeadやRHandなどの正式名称。</returns>
	const char* GetBodyPartName(int index);

	/// <summary>
	/// 編集用正式名または旧モデルボーン名から部位番号を取得する。
	/// </summary>
	/// <param name="name">検索する編集用部位名またはモデルボーン名。</param>
	/// <returns>対応する部位番号。見つからない場合は -1。</returns>
	int FindBodyPartIndex(std::string_view name);

	/// <summary>
	/// MotionDataの部位名を、指定モデルの実ボーン番号へ解決する。
	/// </summary>
	/// <param name="model">検索対象のモデルリソース。</param>
	/// <param name="motionBoneName">編集用部位名または実ボーン名。</param>
	/// <returns>見つかった実ボーン番号。存在しない場合は -1。</returns>
	int FindModelBoneIndex(const ModelResource& model, std::string_view motionBoneName);
}
