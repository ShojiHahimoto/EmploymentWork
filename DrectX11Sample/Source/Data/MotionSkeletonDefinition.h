#pragma once

#include <SimpleMath.h>

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

/// <summary>全姿勢編集で共有する、基準姿勢からのswing(X/Z)と長軸twist(Y)の度数範囲。</summary>
struct MotionPoseRotationLimit
{
	bool enabled = false;
	DirectX::SimpleMath::Vector3 minimum = DirectX::SimpleMath::Vector3(-180, -180, -180);
	DirectX::SimpleMath::Vector3 maximum = DirectX::SimpleMath::Vector3(180, 180, 180);
	float maxSwingDegrees = 175.0f;
};

/// <summary>
/// 標準部位とモデル側ボーンの対応、親部位を保持する。
/// </summary>
struct MotionBodyPartDefinition
{
	MotionBodyPart bodyPart = MotionBodyPart::None;
	const char* editorName = "";
	MotionBodyPart parent = MotionBodyPart::None;
	std::array<const char*, 4> modelBoneNames = {};
	DirectX::SimpleMath::Vector3 editorRotationSign = DirectX::SimpleMath::Vector3::One;
};

namespace MotionSkeleton
{
	MotionPoseRotationLimit GetPoseRotationLimit(MotionBodyPart part);
	const std::array<MotionBodyPartDefinition, MotionBodyPartCount>& GetBodyPartDefinitions();

	const MotionBodyPartDefinition& GetBodyPartDefinition(int index);

	const char* GetBodyPartName(int index);

	DirectX::SimpleMath::Vector3 GetEditorRotationSign(int index);

	int FindBodyPartIndex(std::string_view name);

	int FindModelBoneIndex(const ModelResource& model, std::string_view motionBoneName);
}
