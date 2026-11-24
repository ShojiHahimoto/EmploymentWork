#pragma once

#include "Controller/CustomizePreviewController.h"
#include "Data/AttackData.h"
#include "Data/MotionData.h"
#include "Data/MotionSkeletonDefinition.h"

#include <SimpleMath.h>

#include <array>
#include <string>

/// <summary>
/// カスタマイズ画面の MotionData 下書き、姿勢キー、汎用モーション用オフセットキーを管理する。
/// </summary>
class CustomizeMotionEditorController
{
  public:
	void ConfigurePoseSampling(const CustomizePreviewController& preview, bool attack, AttackUsableState usableState);
	void AddAttackMovementKeyframe(AttackData &attackData, int frame, DirectX::SimpleMath::Vector2 &offset);
	void DeleteAttackMovementKeyframe(AttackData &attackData, int frame, DirectX::SimpleMath::Vector2 &offset);
	void UpdateAttackMovementKeyframe(AttackData &attackData, int frame, const DirectX::SimpleMath::Vector2 &offset);
	bool HasAttackMovementKeyframe(const AttackData &attackData, int frame) const;
	void SelectBodyPart(CustomizePreviewController &preview, int part, int frame);
	std::array<bool, MotionBodyPartCount> GetSelectedBodyParts() const;
	/// <summary>複数選択モードを取得する。</summary>
	/// <returns>複数選択中ならtrue。</returns>
	bool IsMultipleSelectionEnabled() const
	{
		return multipleSelectionEnabled;
	}
	void SetMultipleSelectionEnabled(bool enabled);
	void SelectAllBodyParts();
	bool BeginIkEdit(CustomizePreviewController &preview, int frame);
	std::string UpdateIkEdit(CustomizePreviewController &preview, int frame,
							 const DirectX::SimpleMath::Vector3 &target);
	void EndIkEdit(CustomizePreviewController &preview);
	void ConsumeTranslationEdit(CustomizePreviewController &preview, int frame);
	static constexpr int MotionNameBufferSize = 128;
	static constexpr int MotionEditorBoneCount = MotionBodyPartCount;

	void ResetForAttackMotion();
	void RestoreDraft(const MotionData &restored, int actionFrame);

	std::string LoadDraft(const std::string &motionDataId, const std::string &fallbackDisplayName, int totalFrames,
						  bool commonMotion);

	std::string SaveDraft(const std::string &motionDataId, int totalFrames, bool looping);

	void CopyEditorBuffers(int actionFrame);

	void RefreshFrameEditValues(int actionFrame);
	bool MoveTimelineKey(AttackData &attack, bool commonMotion, bool movement, int source, int destination,
						 int totalFrames);

	DirectX::SimpleMath::Vector2 GetAttackMovementOffsetAtFrame(const AttackData &attackData, int frame) const;

	std::string AddWholeBodyKeyframe(int keyFrame, int totalFrames);

	std::string DeleteWholeBodyKeyframe(int keyFrame);

	std::string SetSelectedRotationKey(CustomizePreviewController &previewController, int keyFrame, int totalFrames);

	std::string ApplyGizmoRotationEdit(int bodyPartIndex, int axisIndex, int keyFrame, int totalFrames,
									   const DirectX::SimpleMath::Quaternion &localRotation);

	bool HasMotionKeyframe(int keyFrame) const;

	std::string AddRootOffsetKeyframe(int keyFrame);

	std::string DeleteRootOffsetKeyframe(int keyFrame);

	std::string SetRootOffsetKeyframe(int keyFrame);

	bool HasRootOffsetKeyframe(int keyFrame) const;

	std::string CopyWholeBodyPose(int keyFrame);

	std::string PasteCopiedPose(int keyFrame, const std::array<bool, MotionEditorBoneCount>& applyMask);

	std::string MirrorPose(const CustomizePreviewController& preview, int frame,
		bool rightToLeft, const std::array<bool, 6>& parts);

	std::string ApplyPosePreset(const std::string &presetId, int keyFrame,
								const std::array<bool, MotionEditorBoneCount> &applyMask);

	MotionData &GetDraft();

	const MotionData &GetDraft() const;

	bool HasDraft() const;

	std::array<char, MotionNameBufferSize> &GetDisplayNameBuffer();

	int &GetSelectedBoneIndex();

	DirectX::SimpleMath::Vector3 &GetRotationEulerDegrees();

	DirectX::SimpleMath::Vector3 &GetRootOffsetKey();

	bool HasCopiedPose() const;

	std::string SavePosePreset(int keyFrame, const std::string &name);

  private:
	std::string CaptureCurrentIkTarget(CustomizePreviewController &previewController, int keyFrame);

	std::string ApplySelectedIkTarget(CustomizePreviewController &previewController, int keyFrame);

	void NormalizeDraftForSave();

	MotionData draft;
	std::array<DirectX::SimpleMath::Quaternion, MotionEditorBoneCount> samplingBaseRotations{};
	bool sampleAttackEndpoints = false;
	DirectX::SimpleMath::Quaternion GetMotionRotationQuaternionAtFrame(
		const MotionData& motion, const std::string& boneName, int frame) const;
	bool hasDraft = false;
	std::array<char, MotionNameBufferSize> displayNameBuffer = {};
	int selectedBoneIndex = 0;
	bool multipleSelectionEnabled = false;
	std::array<bool, MotionBodyPartCount> selectedBodyParts{};
	DirectX::SimpleMath::Vector3 rotationEulerDegrees = DirectX::SimpleMath::Vector3::Zero;
	DirectX::SimpleMath::Vector3 rootOffsetKey = DirectX::SimpleMath::Vector3::Zero;
	std::array<DirectX::SimpleMath::Quaternion, MotionEditorBoneCount> copiedPoseRotations = {};
	bool hasCopiedPose = false;
	DirectX::SimpleMath::Vector3 ikTargetWorldPosition = DirectX::SimpleMath::Vector3::Zero;
	bool hasIkTargetWorldPosition = false;
	bool ikTargetEditing = false; // アクティブな位置入力だけが開始姿勢を保持する。
	int ikTargetKeyFrame = -1;
	int ikTargetBodyPartIndex = -1;
};
