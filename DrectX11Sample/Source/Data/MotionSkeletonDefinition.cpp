#include "Data/MotionSkeletonDefinition.h"

#include "Resource/ModelResource.h"

#include <algorithm>
#include <string>

namespace
{
	/// <summary>
	/// 制限なしの関節可動域を作る。
	/// </summary>
	/// <returns>可動域制限無効の設定。</returns>
	MotionJointRotationLimit NoRotationLimit()
	{
		return MotionJointRotationLimit{};
	}

	/// <summary>
	/// 編集用の甘めの回転可動域を作る。
	/// </summary>
	/// <param name="minX">X軸の最小角度。</param>
	/// <param name="maxX">X軸の最大角度。</param>
	/// <param name="minY">Y軸の最小角度。</param>
	/// <param name="maxY">Y軸の最大角度。</param>
	/// <param name="minZ">Z軸の最小角度。</param>
	/// <param name="maxZ">Z軸の最大角度。</param>
	/// <returns>可動域制限有効の設定。</returns>
	MotionJointRotationLimit RotationLimit(
		float minX,
		float maxX,
		float minY,
		float maxY,
		float minZ,
		float maxZ)
	{
		MotionJointRotationLimit limit;
		limit.enabled = true;
		limit.minDegrees = DirectX::SimpleMath::Vector3(minX, minY, minZ);
		limit.maxDegrees = DirectX::SimpleMath::Vector3(maxX, maxY, maxZ);
		return limit;
	}

	/// <summary>
	/// 内部値と編集UI表示値の符号をそのままにする。
	/// </summary>
	/// <returns>全軸 +1 の符号。</returns>
	DirectX::SimpleMath::Vector3 SameEditorRotationSign()
	{
		return DirectX::SimpleMath::Vector3::One;
	}

	/// <summary>
	/// 左側部位を右側と同じ数値で対称編集しやすくする符号を返す。
	/// </summary>
	/// <returns>Xは同値、Y/Zは反転する符号。</returns>
	DirectX::SimpleMath::Vector3 LeftSideEditorRotationSign()
	{
		return DirectX::SimpleMath::Vector3(1.0f, -1.0f, -1.0f);
	}

	const std::array<MotionBodyPartDefinition, MotionBodyPartCount> BodyPartDefinitions =
	{
		MotionBodyPartDefinition
		{
			MotionBodyPart::Head,
			"Head",
			MotionBodyPart::Spine,
			{ "mixamorig:Head", "Head", "", "" },
			RotationLimit(-60.0f, 60.0f, -80.0f, 80.0f, -45.0f, 45.0f),
			SameEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::Spine,
			"Spine",
			MotionBodyPart::Waist,
			{ "mixamorig:Spine", "mixamorig:Spine1", "Spine", "" },
			RotationLimit(-45.0f, 45.0f, -55.0f, 55.0f, -45.0f, 45.0f),
			SameEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::Waist,
			"Waist",
			MotionBodyPart::None,
			{ "mixamorig:Hips", "Hips", "", "" },
			NoRotationLimit(),
			SameEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::RShoulder,
			"RShoulder",
			MotionBodyPart::Spine,
			{ "mixamorig:RightArm", "RightArm", "mixamorig:RightShoulder", "" },
			RotationLimit(-120.0f, 100.0f, -180.0f, 60.0f, -150.0f, 120.0f),
			SameEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::LShoulder,
			"LShoulder",
			MotionBodyPart::Spine,
			{ "mixamorig:LeftArm", "LeftArm", "mixamorig:LeftShoulder", "" },
			RotationLimit(-120.0f, 100.0f, -60.0f, 180.0f, -120.0f, 150.0f),
			LeftSideEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::RElbow,
			"RElbow",
			MotionBodyPart::RShoulder,
			{ "mixamorig:RightForeArm", "RightForeArm", "", "" },
			RotationLimit(-160.0f, 10.0f, -35.0f, 35.0f, -35.0f, 35.0f),
			SameEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::LElbow,
			"LElbow",
			MotionBodyPart::LShoulder,
			{ "mixamorig:LeftForeArm", "LeftForeArm", "", "" },
			RotationLimit(-160.0f, 10.0f, -35.0f, 35.0f, -35.0f, 35.0f),
			LeftSideEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::RHand,
			"RHand",
			MotionBodyPart::RElbow,
			{ "mixamorig:RightHand", "RightHand", "", "" },
			RotationLimit(-90.0f, 90.0f, -90.0f, 90.0f, -90.0f, 90.0f),
			SameEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::LHand,
			"LHand",
			MotionBodyPart::LElbow,
			{ "mixamorig:LeftHand", "LeftHand", "", "" },
			RotationLimit(-90.0f, 90.0f, -90.0f, 90.0f, -90.0f, 90.0f),
			LeftSideEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::RHipjoint,
			"RHipjoint",
			MotionBodyPart::Waist,
			{ "mixamorig:RightUpLeg", "RightUpLeg", "", "" },
			RotationLimit(-140.0f, 100.0f, -90.0f, 90.0f, -90.0f, 90.0f),
			SameEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::LHipjoint,
			"LHipjoint",
			MotionBodyPart::Waist,
			{ "mixamorig:LeftUpLeg", "LeftUpLeg", "", "" },
			RotationLimit(-140.0f, 100.0f, -90.0f, 90.0f, -90.0f, 90.0f),
			LeftSideEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::RKnees,
			"RKnees",
			MotionBodyPart::RHipjoint,
			{ "mixamorig:RightLeg", "RightLeg", "", "" },
			RotationLimit(-10.0f, 160.0f, -20.0f, 20.0f, -20.0f, 20.0f),
			SameEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::LKnees,
			"LKnees",
			MotionBodyPart::LHipjoint,
			{ "mixamorig:LeftLeg", "LeftLeg", "", "" },
			RotationLimit(-10.0f, 160.0f, -20.0f, 20.0f, -20.0f, 20.0f),
			LeftSideEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::RFeet,
			"RFeet",
			MotionBodyPart::RKnees,
			{ "mixamorig:RightFoot", "RightFoot", "", "" },
			RotationLimit(-80.0f, 80.0f, -60.0f, 60.0f, -60.0f, 60.0f),
			SameEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::LFeet,
			"LFeet",
			MotionBodyPart::LKnees,
			{ "mixamorig:LeftFoot", "LeftFoot", "", "" },
			RotationLimit(-80.0f, 80.0f, -60.0f, 60.0f, -60.0f, 60.0f),
			LeftSideEditorRotationSign()
		}
	};
}

namespace MotionSkeleton
{
	/// <summary>
	/// 15部位の共通定義一覧を取得する。
	/// </summary>
	/// <returns>編集用部位の定義一覧。</returns>
	const std::array<MotionBodyPartDefinition, MotionBodyPartCount>& GetBodyPartDefinitions()
	{
		return BodyPartDefinitions;
	}

	/// <summary>
	/// 部位番号に対応する共通定義を取得する。
	/// </summary>
	/// <param name="index">0から始まる部位番号。</param>
	/// <returns>範囲内へ補正した部位定義。</returns>
	const MotionBodyPartDefinition& GetBodyPartDefinition(int index)
	{
		const int clampedIndex = std::clamp(index, 0, MotionBodyPartCount - 1);
		return BodyPartDefinitions[static_cast<size_t>(clampedIndex)];
	}

	/// <summary>
	/// 部位番号から編集画面用の正式名称を取得する。
	/// </summary>
	/// <param name="index">0から始まる部位番号。</param>
	/// <returns>HeadやRHandなどの正式名称。</returns>
	const char* GetBodyPartName(int index)
	{
		return GetBodyPartDefinition(index).editorName;
	}

	/// <summary>
	/// 部位番号に対応する編集用回転可動域を取得する。
	/// </summary>
	/// <param name="index">0から始まる部位番号。</param>
	/// <returns>有効/無効フラグ付きの回転可動域。</returns>
	const MotionJointRotationLimit& GetRotationLimit(int index)
	{
		return GetBodyPartDefinition(index).rotationLimit;
	}

	/// <summary>
	/// 内部保存値と編集UI表示値を変換するための軸符号を取得する。
	/// </summary>
	/// <param name="index">0から始まる部位番号。</param>
	/// <returns>各軸に掛ける符号。左側部位は対称編集用に一部軸が -1 になる。</returns>
	DirectX::SimpleMath::Vector3 GetEditorRotationSign(int index)
	{
		return GetBodyPartDefinition(index).editorRotationSign;
	}

	/// <summary>
	/// 編集用正式名または旧モデルボーン名から部位番号を取得する。
	/// </summary>
	/// <param name="name">検索する編集用部位名またはモデルボーン名。</param>
	/// <returns>対応する部位番号。見つからない場合は -1。</returns>
	int FindBodyPartIndex(std::string_view name)
	{
		for (int index = 0; index < MotionBodyPartCount; ++index)
		{
			const MotionBodyPartDefinition& definition = BodyPartDefinitions[static_cast<size_t>(index)];
			if (name == definition.editorName)
			{
				return index;
			}

			for (const char* modelBoneName : definition.modelBoneNames)
			{
				if (modelBoneName[0] != '\0' && name == modelBoneName)
				{
					return index;
				}
			}
		}

		return -1;
	}

	/// <summary>
	/// MotionDataの部位名を、指定モデルの実ボーン番号へ解決する。
	/// </summary>
	/// <param name="model">検索対象のモデルリソース。</param>
	/// <param name="motionBoneName">編集用部位名または実ボーン名。</param>
	/// <returns>見つかった実ボーン番号。存在しない場合は -1。</returns>
	int FindModelBoneIndex(const ModelResource& model, std::string_view motionBoneName)
	{
		const int directBoneIndex = model.FindBoneIndex(std::string(motionBoneName));
		if (directBoneIndex >= 0)
		{
			return directBoneIndex;
		}

		const int bodyPartIndex = FindBodyPartIndex(motionBoneName);
		if (bodyPartIndex < 0)
		{
			return -1;
		}

		const MotionBodyPartDefinition& definition = BodyPartDefinitions[static_cast<size_t>(bodyPartIndex)];
		for (const char* modelBoneName : definition.modelBoneNames)
		{
			if (!modelBoneName || modelBoneName[0] == '\0')
			{
				continue;
			}

			const int aliasedBoneIndex = model.FindBoneIndex(modelBoneName);
			if (aliasedBoneIndex >= 0)
			{
				return aliasedBoneIndex;
			}
		}

		return -1;
	}
}
