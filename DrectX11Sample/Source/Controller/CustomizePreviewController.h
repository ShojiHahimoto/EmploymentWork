#pragma once

#include "Component/CameraComponent.h"
#include "Component/TransformComponent.h"
#include "Data/AttackData.h"
#include "Data/MotionData.h"
#include "Data/MotionSkeletonDefinition.h"
#include "Data/SkeletonPose.h"
#include "Customize/MotionIk.h"
#include "Core/Renderer.h"

#include <SimpleMath.h>

#include <array>
#include <string>
#include <vector>

/// <summary>
/// カスタマイズ画面の3Dプレビュー状態と描画を管理する。
/// </summary>
class CustomizePreviewController
{
  public:
	/// <summary>履歴を一つにまとめるため、回転・IK・全身移動ドラッグの継続状態を取得する。</summary>
	/// <returns>いずれかの編集ドラッグ中ならtrue。</returns>
	bool IsGizmoEditing() const
	{
		return rotationGizmoDragActive || translationGizmoActive || rootMovementActive;
	}
	void CancelGizmoEditing();
	// 全身移動キー用。関節IKのターゲットやチェーンには流用しない。
	bool rootMovementGizmoEnabled = false;
	bool rootMovementChanged = false;
	bool rootMovementActive = false;
	DirectX::SimpleMath::Vector3 rootMovementOffset = DirectX::SimpleMath::Vector3::Zero;
	DirectX::SimpleMath::Matrix rootMovementMatrix = DirectX::SimpleMath::Matrix::Identity;
	DirectX::SimpleMath::Vector3 rootMovementStartOffset = DirectX::SimpleMath::Vector3::Zero;
	DirectX::SimpleMath::Vector3 rootMovementStartHandle = DirectX::SimpleMath::Vector3::Zero;
	std::vector<RECT> editorToolbarExclusions; // 実際に表示したUI矩形だけを除外する。
	bool IsOverEditorToolbar(POINT point) const;
	bool editorInputBlocked = false;
	bool showJointMarkers = true; // 非表示中は関節のヒットテストも行わない。Gizmoは維持する。
	std::array<bool, MotionBodyPartCount>
		selectedMarkerParts{}; // 複数選択の表示。Gizmoはアクティブ部位だけを操作する。
	int previousGizmoFrame = -100000;
	int previousGizmoMode = -1;
	int previousGizmoPart = -1;
	void ResetCamera();
	void DrawCameraResetButton();
	bool translationGizmoEnabled = false;
	bool translationGizmoActive = false;
	bool hasTranslationGizmoEdit = false;
	MotionIkResult translationGizmoResult;
	DirectX::SimpleMath::Matrix translationGizmoMatrix = DirectX::SimpleMath::Matrix::Identity;
	int translationGizmoPart = -1;
	int translationGizmoFrame = -1;
	static constexpr int PreviewTextureWidth = 640;
	static constexpr int PreviewTextureHeight = 360;

	void Initialize();

	void Release();

	bool UpdatePlayback(bool enabled, bool looping, int totalFrames);

	void Play(int totalFrames);

	void Stop();

	void StepFrame(int frameDelta, int totalFrames);

	void ClampCurrentFrame(int totalFrames);

	void Render(Renderer &renderer, const RECT *region, const AttackData &attackData, const MotionData &motionData,
				bool editingCommonMotion, bool hasDraftMotion, const std::string &editingMotionDataId,
				int selectedBodyPartIndex, bool rotationGizmoEnabled);

	bool ConsumeGizmoRotationEdit(int &outBodyPartIndex, int &outAxisIndex,
								  DirectX::SimpleMath::Quaternion &outLocalRotation);

	bool BeginIkSolveSession(int actionFrame);

	bool GetBodyPartBindRotation(int part, DirectX::SimpleMath::Quaternion &rotation) const;

	bool GetBodyPartBindParentBasis(int part, DirectX::SimpleMath::Matrix& basis) const;

	void EndIkSolveSession();

	bool GetBodyPartWorldPosition(int bodyPartIndex, DirectX::SimpleMath::Vector3 &outWorldPosition) const;

	bool SolveIkTarget(int targetBodyPartIndex, const DirectX::SimpleMath::Vector3 &targetWorldPosition,
					   MotionIkResult &result);

	int ConsumePickedBodyPartIndex();

	int GetCurrentFrame() const;

	void SetCurrentFrame(int frame);

	int GetActionFrame() const;

	bool HasRenderedActionFrame(int actionFrame) const;

	bool IsPlaying() const;

	const Renderer::RenderTexture &GetRenderTexture() const;

	float &GetCameraYawDegrees();

	float &GetCameraPitchDegrees();

	float &GetCameraDistance();

  private:
	void UpdateCameraInput(const RECT &region);
	void UpdateCameraTransform();

	void DrawAttackBoxes(Renderer &renderer, const AttackData &attackData, bool editingCommonMotion);

	void DrawJointMarkers(const RECT &region, const ModelResource &model, int selectedBodyPartIndex);

	void DrawRotationGizmo(const RECT &region, const ModelResource &model, int selectedBodyPartIndex, bool enabled);

	Renderer::RenderTexture renderTexture;
	CameraComponent camera;
	TransformComponent cameraTransform;
	TransformComponent playerTransform;
	SkeletonPose skeletonPose;
	SkeletonPose ikSolveBasePose;
	// 直前に採用した姿勢。開始姿勢とは別に保持して解の連続性を維持する。
	SkeletonPose ikSolveAcceptedPose;
	DirectX::SimpleMath::Matrix ikSolveObjectWorld = DirectX::SimpleMath::Matrix::Identity;
	int currentFrame = 0;
	int lastRenderedActionFrame = -100000;
	bool hasIkSolveBasePose = false;
	bool playing = false;
	int pickedBodyPartIndex = -1;
	bool hasGizmoRotationEdit = false;
	int gizmoEditedBodyPartIndex = -1;
	int gizmoEditedAxisIndex = -1;
	DirectX::SimpleMath::Quaternion gizmoLocalRotation = DirectX::SimpleMath::Quaternion::Identity;
	bool rotationGizmoDragActive = false;
	int rotationGizmoDragBodyPartIndex = -1;
	int rotationGizmoDragAxisIndex = -1;
	DirectX::SimpleMath::Matrix rotationGizmoDragWorldMatrix = DirectX::SimpleMath::Matrix::Identity;
	DirectX::SimpleMath::Quaternion rotationGizmoDragStartLocalRotation = DirectX::SimpleMath::Quaternion::Identity;
	float rotationGizmoDragAccumulatedRadians = 0.0f;
	// ドラッグ開始時の共通可動域から求めた1軸角度範囲。毎描画フレームで再探索しない。
	DirectX::SimpleMath::Vector2 rotationGizmoDragLimits = DirectX::SimpleMath::Vector2::Zero;
	float cameraYawDegrees = 0.0f;
	float cameraPitchDegrees = -2.5f;
	float cameraDistance = 14.0f;
	float actualCameraDistance = 14.0f; // 壁補正後の距離。cameraDistanceはユーザー指定値として保持する。
	// カメラ操作だけの状態。モーションや本体Transformには保存しない。
	DirectX::SimpleMath::Vector3 cameraTargetOffset = DirectX::SimpleMath::Vector3::Zero;
	DirectX::SimpleMath::Vector3 cameraDragTarget = DirectX::SimpleMath::Vector3::Zero;
	int cameraDragButton = -1;
};
