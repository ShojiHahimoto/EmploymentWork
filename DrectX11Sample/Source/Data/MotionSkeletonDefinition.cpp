#include "Data/MotionSkeletonDefinition.h"

#include "Resource/ModelResource.h"

#include <algorithm>
#include <string>

namespace
{
	const std::array<MotionBodyPartDefinition, MotionBodyPartCount> BodyPartDefinitions =
	{
		MotionBodyPartDefinition
		{
			MotionBodyPart::Head,
			"Head",
			MotionBodyPart::Spine,
			{ "mixamorig:Head", "Head", "", "" }
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::Spine,
			"Spine",
			MotionBodyPart::Waist,
			{ "mixamorig:Spine", "mixamorig:Spine1", "Spine", "" }
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::Waist,
			"Waist",
			MotionBodyPart::None,
			{ "mixamorig:Hips", "Hips", "", "" }
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::RShoulder,
			"RShoulder",
			MotionBodyPart::Spine,
			{ "mixamorig:RightArm", "RightArm", "mixamorig:RightShoulder", "" }
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::LShoulder,
			"LShoulder",
			MotionBodyPart::Spine,
			{ "mixamorig:LeftArm", "LeftArm", "mixamorig:LeftShoulder", "" }
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::RElbow,
			"RElbow",
			MotionBodyPart::RShoulder,
			{ "mixamorig:RightForeArm", "RightForeArm", "", "" }
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::LElbow,
			"LElbow",
			MotionBodyPart::LShoulder,
			{ "mixamorig:LeftForeArm", "LeftForeArm", "", "" }
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::RHand,
			"RHand",
			MotionBodyPart::RElbow,
			{ "mixamorig:RightHand", "RightHand", "", "" }
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::LHand,
			"LHand",
			MotionBodyPart::LElbow,
			{ "mixamorig:LeftHand", "LeftHand", "", "" }
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::RHipjoint,
			"RHipjoint",
			MotionBodyPart::Waist,
			{ "mixamorig:RightUpLeg", "RightUpLeg", "", "" }
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::LHipjoint,
			"LHipjoint",
			MotionBodyPart::Waist,
			{ "mixamorig:LeftUpLeg", "LeftUpLeg", "", "" }
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::RKnees,
			"RKnees",
			MotionBodyPart::RHipjoint,
			{ "mixamorig:RightLeg", "RightLeg", "", "" }
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::LKnees,
			"LKnees",
			MotionBodyPart::LHipjoint,
			{ "mixamorig:LeftLeg", "LeftLeg", "", "" }
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::RFeet,
			"RFeet",
			MotionBodyPart::RKnees,
			{ "mixamorig:RightFoot", "RightFoot", "", "" }
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::LFeet,
			"LFeet",
			MotionBodyPart::LKnees,
			{ "mixamorig:LeftFoot", "LeftFoot", "", "" }
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
