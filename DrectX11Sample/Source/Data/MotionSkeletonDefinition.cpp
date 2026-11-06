#include "Data/MotionSkeletonDefinition.h"

#include "Resource/ModelResource.h"

#include <algorithm>
#include <string>

namespace
{
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
			SameEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::Spine,
			"Spine",
			MotionBodyPart::Waist,
			{ "mixamorig:Spine", "mixamorig:Spine1", "Spine", "" },
			SameEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::Waist,
			"Waist",
			MotionBodyPart::None,
			{ "mixamorig:Hips", "Hips", "", "" },
			SameEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::RShoulder,
			"RShoulder",
			MotionBodyPart::Spine,
			{ "mixamorig:RightArm", "RightArm", "mixamorig:RightShoulder", "" },
			SameEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::LShoulder,
			"LShoulder",
			MotionBodyPart::Spine,
			{ "mixamorig:LeftArm", "LeftArm", "mixamorig:LeftShoulder", "" },
			LeftSideEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::RElbow,
			"RElbow",
			MotionBodyPart::RShoulder,
			{ "mixamorig:RightForeArm", "RightForeArm", "", "" },
			SameEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::LElbow,
			"LElbow",
			MotionBodyPart::LShoulder,
			{ "mixamorig:LeftForeArm", "LeftForeArm", "", "" },
			LeftSideEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::RHand,
			"RHand",
			MotionBodyPart::RElbow,
			{ "mixamorig:RightHand", "RightHand", "", "" },
			SameEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::LHand,
			"LHand",
			MotionBodyPart::LElbow,
			{ "mixamorig:LeftHand", "LeftHand", "", "" },
			LeftSideEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::RHipjoint,
			"RHipjoint",
			MotionBodyPart::Waist,
			{ "mixamorig:RightUpLeg", "RightUpLeg", "", "" },
			SameEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::LHipjoint,
			"LHipjoint",
			MotionBodyPart::Waist,
			{ "mixamorig:LeftUpLeg", "LeftUpLeg", "", "" },
			LeftSideEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::RKnees,
			"RKnees",
			MotionBodyPart::RHipjoint,
			{ "mixamorig:RightLeg", "RightLeg", "", "" },
			SameEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::LKnees,
			"LKnees",
			MotionBodyPart::LHipjoint,
			{ "mixamorig:LeftLeg", "LeftLeg", "", "" },
			LeftSideEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::RFeet,
			"RFeet",
			MotionBodyPart::RKnees,
			{ "mixamorig:RightFoot", "RightFoot", "", "" },
			SameEditorRotationSign()
		},
		MotionBodyPartDefinition
		{
			MotionBodyPart::LFeet,
			"LFeet",
			MotionBodyPart::LKnees,
			{ "mixamorig:LeftFoot", "LeftFoot", "", "" },
			LeftSideEditorRotationSign()
		}
	};
}

namespace MotionSkeleton
{
	MotionPoseRotationLimit GetPoseRotationLimit(MotionBodyPart part)
	{
		using Part = MotionBodyPart;
		// Mixamoの基準骨軸Yを長軸とする。X/Zはswing回転ベクトルの成分。
		// Tポーズ基準なので、人体の腕を下げた中立姿勢の角度を直接代入しない。
		// 肩は広めの円錐、前腕は屈曲と回内外を分離。値は編集用で医療的な限界ではない。
		switch (part)
		{
		case Part::RShoulder: case Part::LShoulder:
			return {true, {-170,-130,-170}, {170,130,170}, 175};
		case Part::RHipjoint: case Part::LHipjoint:
			return {true, {-155,-100,-110}, {115,100,110}, 165};
		case Part::RElbow: case Part::LElbow:
			return {true, {-170,-110,-15}, {0,110,15}, 175};
		case Part::RKnees: case Part::LKnees:
			return {true, {0,-25,-10}, {165,25,10}, 170};
		case Part::RHand: case Part::LHand:
			return {true, {-100,-100,-60}, {100,100,60}, 120};
		case Part::RFeet: case Part::LFeet:
			return {true, {-85,-65,-50}, {85,65,50}, 100};
		case Part::Head:
			return {true, {-70,-90,-55}, {70,90,55}, 100};
		case Part::Spine:
			return {true, {-55,-70,-55}, {55,70,55}, 85};
		default: return {};
		}
	}

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
