#include "Customize/MotionPose.h"
#include <cassert>
#include <cmath>
#include <iostream>

using namespace DirectX::SimpleMath;

// この単体テストでは GPU/FBX ローダーをリンクせず、実親階層を持つ固定モデルを供給する。
static std::vector<ModelBone> testBones;
const std::vector<ModelBone> &ModelResource::GetBones() const
{
	return testBones;
}
int ModelResource::FindBoneIndex(const std::string &name) const
{
	for (size_t i = 0; i < testBones.size(); ++i)
	{
		if (testBones[i].name == name)
		{
			return static_cast<int>(i);
		}
	}
	return -1;
}
ModelMesh::~ModelMesh() = default;
ModelMaterial::~ModelMaterial() = default;

/// <summary>浮動小数点の誤差を許容して行列を比較する。</summary>
/// <param name="a">実際の結果。</param>
/// <param name="b">期待する結果。</param>
static void CheckMatrix(const Matrix &a, const Matrix &b)
{
	for (int row = 0; row < 4; ++row)
	{
		for (int col = 0; col < 4; ++col)
		{
			assert(std::abs(a.m[row][col] - b.m[row][col]) < 0.001f);
		}
	}
}

int main()
{
	ModelResource model;
	testBones.resize(3);
	for (int i = 0; i < 3; ++i)
	{
		testBones[i].name = "bone" + std::to_string(i);
		testBones[i].parentIndex = i - 1;
		testBones[i].bindLocalPosition = Vector3(0, 1, 0);
	}
	SkeletonPose pose;
	assert(MotionPose::InitializeSkeletonPose(pose, model, "fixture"));
	CheckMatrix(pose.boneModelMatrices[2], Matrix::CreateTranslation(0, 3, 0));

	// 親回転は子孫へ伝播する。子のローカル姿勢そのものは変更しない。
	assert(MotionPose::SetBoneLocalEulerRotationDegrees(pose, model, "bone0", Vector3(0, 0, 90)));
	MotionPose::UpdateSkinningMatrices(pose, model);
	const Matrix root = pose.boneModelMatrices[0];
	const Matrix child = pose.boneModelMatrices[1];
	CheckMatrix(child, Matrix::CreateTranslation(0, 1, 0) * root);
	CheckMatrix(pose.boneModelMatrices[2], Matrix::CreateTranslation(0, 1, 0) * child);
	assert(pose.bonePoses[1].localRotation == Quaternion::Identity);

	assert(MotionPose::SetBoneLocalEulerRotationDegrees(pose, model, "bone2", Vector3(20, 30, 40)));
	MotionPose::UpdateSkinningMatrices(pose, model);
	CheckMatrix(pose.boneModelMatrices[0], root);
	CheckMatrix(pose.boneModelMatrices[1], child);

	// モデル本体の回転・拡縮・移動を含めても、親基準のローカル姿勢へ戻せる。
	const Matrix objectWorld =
		Matrix::CreateScale(2) * Matrix::CreateRotationY(0.4f) * Matrix::CreateTranslation(5, 6, 7);
	for (int i = 0; i < 3; ++i)
	{
		Matrix world;
		BonePose restored;
		assert(MotionPose::GetBoneWorldMatrix(pose, i, objectWorld, world));
		assert(MotionPose::WorldToLocalPose(pose, model, i, objectWorld, world, restored));
		const BonePose &expected = pose.bonePoses[i];
		CheckMatrix(Matrix::CreateFromQuaternion(restored.localRotation),
					Matrix::CreateFromQuaternion(expected.localRotation));
		assert(Vector3::Distance(restored.localPosition, expected.localPosition) < 0.001f);
		assert(Vector3::Distance(restored.localScale, expected.localScale) < 0.001f);
	}
	BonePose unchanged;
	unchanged.localPosition = Vector3(9, 8, 7);
	assert(!MotionPose::WorldToLocalPose(pose, model, 1, Matrix::CreateScale(0), Matrix::Identity, unchanged));
	assert(unchanged.localPosition == Vector3(9, 8, 7));

	// キー間の補間、ループ、未指定部位の下地を共通計算経路で確認する。
	MotionData motion;
	motion.totalFrames = 8;
	MotionBoneTrackData track;
	track.boneName = "bone1";
	MotionBoneKeyframeData start;
	start.frame = 0;
	start.hasRotation = true;
	start.localRotation = Quaternion::Identity;
	MotionBoneKeyframeData end = start;
	end.frame = 4;
	end.localRotation = Quaternion::CreateFromYawPitchRoll(0, 0, 1.0f);
	track.keyframes = {start, end};
	motion.boneTracks.push_back(track);
	SkeletonPose sampled;
	MotionPose::ApplyMotionData(sampled, motion, 2, model, &pose);
	CheckMatrix(Matrix::CreateFromQuaternion(sampled.bonePoses[1].localRotation),
				Matrix::CreateFromQuaternion(Quaternion::Slerp(start.localRotation, end.localRotation, 0.5f)));
	assert(sampled.bonePoses[0].localRotation == pose.bonePoses[0].localRotation);
	motion.looping = true;
	MotionPose::ApplyMotionData(sampled, motion, 6, model);
	CheckMatrix(Matrix::CreateFromQuaternion(sampled.bonePoses[1].localRotation),
				Matrix::CreateFromQuaternion(Quaternion::Slerp(end.localRotation, start.localRotation, 0.5f)));
	// 地上・空中それぞれの基準姿勢で、前後の補間と追加直後の一致を確認する。
	for (const float baseAngle : {0.2f, 0.7f})
	{
		BonePose base;
		base.localRotation = Quaternion::CreateFromYawPitchRoll(baseAngle, 0, 0);
		track.keyframes = {end};
		track.keyframes.front().frame = 4;

		const auto before = MotionPose::SampleBoneTrack(track, base, 1, 10, false, true);
		CheckMatrix(Matrix::CreateFromQuaternion(before.localRotation),
					Matrix::CreateFromQuaternion(Quaternion::Slerp(base.localRotation, end.localRotation, 2.0f / 5)));

		const auto after = MotionPose::SampleBoneTrack(track, base, 7, 10, false, true);
		CheckMatrix(Matrix::CreateFromQuaternion(after.localRotation),
					Matrix::CreateFromQuaternion(Quaternion::Slerp(end.localRotation, base.localRotation, 3.0f / 5)));

		for (const int frame : {1, 4, 7, 9})
		{
			auto copy = track;
			const auto original = MotionPose::SampleBoneTrack(copy, base, frame, 10, false, true);
			MotionBoneKeyframeData inserted = end;
			inserted.frame = frame;
			inserted.localRotation = original.localRotation;
			copy.keyframes.erase(std::remove_if(copy.keyframes.begin(), copy.keyframes.end(),
												[frame](const auto &key)
												{
													return key.frame == frame;
												}),
								 copy.keyframes.end());
			copy.keyframes.push_back(inserted);
			std::sort(copy.keyframes.begin(), copy.keyframes.end(),
					  [](const auto &a, const auto &b)
					  {
						  return a.frame < b.frame;
					  });

			const auto added = MotionPose::SampleBoneTrack(copy, base, frame, 10, false, true);
			CheckMatrix(Matrix::CreateFromQuaternion(added.localRotation),
						Matrix::CreateFromQuaternion(original.localRotation));
		}

		CheckMatrix(
			Matrix::CreateFromQuaternion(MotionPose::SampleBoneTrack(track, base, 9, 10, false, true).localRotation),
			Matrix::CreateFromQuaternion(base.localRotation));
		track.keyframes.front().frame = 9;
		CheckMatrix(
			Matrix::CreateFromQuaternion(MotionPose::SampleBoneTrack(track, base, 9, 10, false, true).localRotation),
			Matrix::CreateFromQuaternion(end.localRotation));
		track.keyframes.clear();
		assert(MotionPose::SampleBoneTrack(track, base, 5, 10, false, true).localRotation == base.localRotation);
	}

	assert(std::string(MotionPose::GetAttackBaseMotionId(AttackUsableState::Air)) == "Common/JumpLoop");
	assert(std::string(MotionPose::GetAttackBaseMotionId(AttackUsableState::Ground)) == "Common/Idle");
	std::cout << "MotionPose tests passed (FK, sampling, loop, attack endpoints, key insertion).\n";
}
