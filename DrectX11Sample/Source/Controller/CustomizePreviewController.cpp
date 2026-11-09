#include "Controller/CustomizePreviewController.h"

#include "Data/MotionDataLoader.h"
#include "Resource/ModelResource.h"
#include "System/CameraSystem.h"
#include "System/Debugger.h"
#include "System/MotionPose.h"
#include "System/TransformSystem.h"
#include "System/imgui-docking/imgui.h"
#include "ThirdParty/ImGuizmo/ImGuizmo.h"

#include <DirectXMath.h>

#include "System/MotionRotationLimits.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

using namespace DirectX;
using namespace DirectX::SimpleMath;

namespace
{
	constexpr const char* CommonIdleMotionDataId = "Common/Idle";
	constexpr const char* PreviewModelKey = "CustomizePreviewPlayer";
	constexpr const char* PreviewModelPath = "assets/model/DebugPlayer/man.fbx";
	constexpr float PreviewBoxDepth = 0.08f;
	constexpr float JointMarkerRadius = 7.0f;
	constexpr float JointPickRadius = 14.0f;
	constexpr float RotationGizmoSizeClipSpace = 0.2f;

	/// <summary>
	/// プレビューキャラの基準位置を取得する。
	/// </summary>
	/// <returns>プレビュー空間上の基準位置。</returns>
	Vector3 GetPreviewPlayerBasePosition()
	{
		return Vector3(0.0f, 0.0f, 8.0f);
	}

	/// <summary>
	/// 攻撃開始位置から見た相対移動量を、movementKeys から線形補間して取得する。
	/// </summary>
	/// <param name="attackData">参照する AttackData。</param>
	/// <param name="frame">内部 0 始まりの攻撃フレーム。</param>
	/// <returns>攻撃開始地点からの相対移動量。</returns>
	Vector2 GetAttackMovementOffsetAtFrameForPreview(const AttackData& attackData, int frame)
	{
		if (attackData.movementKeys.empty() || frame < 0)
		{
			return Vector2::Zero;
		}

		const AttackMovementKeyData* previousKey = nullptr;
		const AttackMovementKeyData* nextKey = nullptr;
		for (const AttackMovementKeyData& keyframe : attackData.movementKeys)
		{
			if (keyframe.frame <= frame)
			{
				previousKey = &keyframe;
			}
			if (keyframe.frame >= frame)
			{
				nextKey = &keyframe;
				break;
			}
		}

		if (!previousKey && nextKey)
		{
			if (nextKey->frame <= 0)
			{
				return nextKey->offset;
			}

			const float rate = std::clamp(
				static_cast<float>(frame) / static_cast<float>(nextKey->frame),
				0.0f,
				1.0f);
			return Vector2::Lerp(Vector2::Zero, nextKey->offset, rate);
		}
		if (previousKey && !nextKey)
		{
			return previousKey->offset;
		}
		if (previousKey && nextKey && previousKey->frame != nextKey->frame)
		{
			const float rate = std::clamp(
				static_cast<float>(frame - previousKey->frame) / static_cast<float>(nextKey->frame - previousKey->frame),
				0.0f,
				1.0f);
			return Vector2::Lerp(previousKey->offset, nextKey->offset, rate);
		}
		if (previousKey)
		{
			return previousKey->offset;
		}

		return Vector2::Zero;
	}

	/// <summary>
	/// 汎用 MotionData の全身見た目オフセットを rootOffsetKeys から線形補間して取得する。
	/// </summary>
	/// <param name="motionData">参照する MotionData。</param>
	/// <param name="frame">内部 0 始まりのモーションフレーム。</param>
	/// <returns>Transform ではなくモデル描画だけに使うオフセット。</returns>
	Vector3 GetMotionRootOffsetAtFrameForPreview(const MotionData& motionData, int frame)
	{
		if (motionData.rootOffsetKeys.empty() || frame < 0)
		{
			return Vector3::Zero;
		}

		const MotionRootOffsetKeyData* previousKey = nullptr;
		const MotionRootOffsetKeyData* nextKey = nullptr;
		for (const MotionRootOffsetKeyData& keyframe : motionData.rootOffsetKeys)
		{
			if (keyframe.frame <= frame)
			{
				previousKey = &keyframe;
			}
			if (keyframe.frame >= frame)
			{
				nextKey = &keyframe;
				break;
			}
		}

		if (!previousKey && nextKey)
		{
			if (nextKey->frame <= 0)
			{
				return nextKey->offset;
			}

			const float rate = std::clamp(
				static_cast<float>(frame) / static_cast<float>(nextKey->frame),
				0.0f,
				1.0f);
			return Vector3::Lerp(Vector3::Zero, nextKey->offset, rate);
		}
		if (motionData.looping && previousKey && !nextKey)
		{
			const MotionRootOffsetKeyData& firstKey = motionData.rootOffsetKeys.front();
			const int totalFrames = std::max(1, motionData.totalFrames);
			const int frameSpan = (totalFrames - previousKey->frame) + firstKey.frame;
			const int frameOffset = frame - previousKey->frame;
			if (frameSpan <= 0)
			{
				return previousKey->offset;
			}

			const float rate = std::clamp(
				static_cast<float>(frameOffset) / static_cast<float>(frameSpan),
				0.0f,
				1.0f);
			return Vector3::Lerp(previousKey->offset, firstKey.offset, rate);
		}
		if (previousKey && !nextKey)
		{
			return previousKey->offset;
		}
		if (previousKey && nextKey && previousKey->frame != nextKey->frame)
		{
			const float rate = std::clamp(
				static_cast<float>(frame - previousKey->frame) / static_cast<float>(nextKey->frame - previousKey->frame),
				0.0f,
				1.0f);
			return Vector3::Lerp(previousKey->offset, nextKey->offset, rate);
		}
		if (previousKey)
		{
			return previousKey->offset;
		}

		return Vector3::Zero;
	}

	/// <summary>
	/// ワールド座標をプレビュー矩形上のスクリーン座標へ投影する。
	/// </summary>
	/// <param name="worldPosition">投影するワールド座標。</param>
	/// <param name="view">現在のビュー行列。</param>
	/// <param name="projection">現在のプロジェクション行列。</param>
	/// <param name="region">プレビュー矩形。</param>
	/// <param name="outScreenPosition">ImGui のスクリーン座標。</param>
	/// <returns>画面内へ投影できた場合は true。</returns>
	bool ProjectToPreviewScreen(
		const Vector3& worldPosition,
		const Matrix& view,
		const Matrix& projection,
		const RECT& region,
		ImVec2& outScreenPosition)
	{
		const Vector3 clipPosition = Vector3::Transform(worldPosition, view * projection);
		if (clipPosition.z < 0.0f || clipPosition.z > 1.0f)
		{
			return false;
		}

		const float regionWidth = static_cast<float>(region.right - region.left);
		const float regionHeight = static_cast<float>(region.bottom - region.top);
		if (regionWidth <= 0.0f || regionHeight <= 0.0f)
		{
			return false;
		}

		const ImVec2 viewportOrigin = ImGui::GetMainViewport()->Pos;
		outScreenPosition.x = viewportOrigin.x + static_cast<float>(region.left) + (clipPosition.x + 1.0f) * 0.5f * regionWidth;
		outScreenPosition.y = viewportOrigin.y + static_cast<float>(region.top) + (1.0f - clipPosition.y) * 0.5f * regionHeight;
		return true;
	}

	/// <summary>
	/// ImGuizmo の回転ハンドル種別を、ローカル回転軸番号へ変換する。
	/// </summary>
	/// <param name="handleType">現在操作中の ImGuizmo ハンドル。</param>
	/// <returns>X=0, Y=1, Z=2。対象外なら -1。</returns>
	int GetRotationAxisIndex(ImGuizmo::MOVETYPE handleType)
	{
		switch (handleType)
		{
		case ImGuizmo::MT_ROTATE_X:
			return 0;
		case ImGuizmo::MT_ROTATE_Y:
			return 1;
		case ImGuizmo::MT_ROTATE_Z:
			return 2;
		default:
			return -1;
		}
	}

	/// <summary>
	/// 回転軸番号からローカル空間の単位軸を取得する。
	/// </summary>
	/// <param name="axisIndex">X=0, Y=1, Z=2。</param>
	/// <returns>ローカル空間の単位軸。</returns>
	Vector3 GetLocalAxisByIndex(int axisIndex)
	{
		switch (axisIndex)
		{
		case 0:
			return Vector3::UnitX;
		case 1:
			return Vector3::UnitY;
		case 2:
			return Vector3::UnitZ;
		default:
			return Vector3::Zero;
		}
	}

	/// <summary>
	/// モデルと現在姿勢から、編集用部位の実ボーンワールド行列を取得する。
	/// </summary>
	/// <param name="pose">参照する姿勢。</param>
	/// <param name="model">部位名から実ボーンを解決するモデル。</param>
	/// <param name="playerWorld">プレビューキャラ本体のワールド行列。</param>
	/// <param name="bodyPartIndex">取得する編集用部位番号。</param>
	/// <param name="outModelBoneIndex">解決した実ボーン番号。</param>
	/// <param name="outWorld">取得した実ワールド行列。</param>
	/// <returns>有効な実ボーン行列を取得できた場合は true。</returns>
	bool GetBodyPartWorldMatrix(
		const SkeletonPose& pose,
		const ModelResource& model,
		const Matrix& playerWorld,
		int bodyPartIndex,
		int& outModelBoneIndex,
		Matrix& outWorld)
	{
		if (bodyPartIndex < 0 || bodyPartIndex >= MotionBodyPartCount)
		{
			return false;
		}

		outModelBoneIndex = MotionSkeleton::FindModelBoneIndex(
			model,
			MotionSkeleton::GetBodyPartName(bodyPartIndex));
		if (outModelBoneIndex < 0 || outModelBoneIndex >= static_cast<int>(pose.boneWorldMatrices.size()))
		{
			return false;
		}

		return MotionPose::GetBoneWorldMatrix(pose, outModelBoneIndex, playerWorld, outWorld);
	}

}

void CustomizePreviewController::Initialize()
{
	Release();

	ModelResourceManager::LoadModel(
		PreviewModelKey,
		PreviewModelPath,
		Renderer::GetDevice());
	if (const ModelResource* previewModel = ModelResourceManager::GetModel(PreviewModelKey))
	{
		MotionPose::InitializeSkeletonPose(skeletonPose, *previewModel, PreviewModelKey);
	}

	TransformSystem::SetLocalPosition(playerTransform, GetPreviewPlayerBasePosition());
	TransformSystem::SetLocalEulerRotationDegrees(playerTransform, Vector3(0.0f, -90.0f, 0.0f));
	TransformSystem::SetLocalScale(playerTransform, Vector3(0.05f, 0.05f, 0.05f));
	TransformSystem::UpdateWorldTransform(playerTransform);

	UpdateCameraTransform();
	TransformSystem::SetLocalScale(cameraTransform, Vector3::One);
	TransformSystem::UpdateWorldTransform(cameraTransform);

	const float aspectRatio = static_cast<float>(PreviewTextureWidth) / PreviewTextureHeight;
	CameraSystem::SetPerspective(camera, 45.0f, aspectRatio, 0.1f, 1000.0f);
	CameraSystem::Update(camera, cameraTransform);

	const HRESULT hr = Renderer::CreateRenderTexture(renderTexture, PreviewTextureWidth, PreviewTextureHeight);
	if (FAILED(hr))
	{
		DebugLog("[CustomizePreviewController] RenderTexture creation failed. hr=", static_cast<long>(hr));
	}
}

void CustomizePreviewController::Release()
{
	ResetCamera();
	Renderer::ReleaseRenderTexture(renderTexture);
}

/// <summary>カメラだけを既定の表示へ戻す。編集姿勢と再生フレームは変更しない。</summary>
void CustomizePreviewController::ResetCamera()
{
	cameraYawDegrees = 0.0f;
	cameraPitchDegrees = -2.5f;
	cameraDistance = 14.0f;
	cameraTargetOffset = Vector3::Zero;
	cameraDragTarget = Vector3::Zero;
	cameraDragButton = -1;
}

/// <summary>再生操作欄に配置するカメラ初期化ボタンを描画する。</summary>
void CustomizePreviewController::DrawCameraResetButton()
{
	if (ImGui::Button("Reset Camera")) ResetCamera();
}

bool CustomizePreviewController::UpdatePlayback(bool enabled, bool looping, int totalFrames)
{
	if (!enabled || !playing)
	{
		return false;
	}

	++currentFrame;
	if (currentFrame >= totalFrames)
	{
		if (looping)
		{
			currentFrame = 1;
		}
		else
		{
			currentFrame = totalFrames;
			playing = false;
		}
	}

	return true;
}

void CustomizePreviewController::Play(int totalFrames)
{
	if (currentFrame >= totalFrames)
	{
		currentFrame = 0;
	}
	playing = true;
}

void CustomizePreviewController::Stop()
{
	playing = false;
}

void CustomizePreviewController::StepFrame(int frameDelta, int totalFrames)
{
	playing = false;
	currentFrame += frameDelta;
	ClampCurrentFrame(totalFrames);
}

void CustomizePreviewController::ClampCurrentFrame(int totalFrames)
{
	currentFrame = std::clamp(currentFrame, 0, totalFrames);
}

void CustomizePreviewController::Render(
	Renderer& renderer,
	const RECT* region,
	const AttackData& attackData,
	const MotionData& motionData,
	bool editingCommonMotion,
	bool hasDraftMotion,
	const std::string& editingMotionDataId,
	int selectedBodyPartIndex,
	bool rotationGizmoEnabled)
{
	hasGizmoRotationEdit = false;
	if (!region && !renderTexture.renderTargetView)
	{
		return;
	}

	const Color defaultClearColor = Renderer::GetDefaultClearColor();
	const float clearColor[4] = {
		defaultClearColor.x,
		defaultClearColor.y,
		defaultClearColor.z,
		defaultClearColor.w
	};
	std::optional<Renderer::ScopedRenderRegion> regionScope;
	if (region)
	{
		regionScope.emplace(*region, true);
		CameraSystem::SetAspectRatio(camera,
			static_cast<float>(region->right - region->left) / static_cast<float>(region->bottom - region->top));
	}
	else
	{
		Renderer::BeginRenderTexture(renderTexture, clearColor);
		CameraSystem::SetAspectRatio(camera, static_cast<float>(PreviewTextureWidth) / PreviewTextureHeight);
	}

	const int actionFrame = GetActionFrame();
	const Vector2 movementOffset = !editingCommonMotion && actionFrame >= 0
		? GetAttackMovementOffsetAtFrameForPreview(attackData, actionFrame)
		: Vector2::Zero;
	const Vector3 visualOffset = editingCommonMotion && actionFrame >= 0
		? GetMotionRootOffsetAtFrameForPreview(motionData, actionFrame)
		: Vector3::Zero;
	TransformSystem::SetLocalPosition(
		playerTransform,
		GetPreviewPlayerBasePosition()
		+ Vector3(movementOffset.x, movementOffset.y, 0.0f)
		+ visualOffset);

	if (region) UpdateCameraInput(*region);
	UpdateCameraTransform();
	TransformSystem::UpdateWorldTransform(playerTransform);
	TransformSystem::UpdateWorldTransform(cameraTransform);
	CameraSystem::Update(camera, cameraTransform);
	renderer.SetViewProjection(camera.viewMatrix, camera.projectionMatrix);
	// 編集画面専用の空間寸法。バトルのステージ境界とは共有せず、
	// プレビューのカメラ・深度・描画領域をそのまま使って床と壁を描く。
	constexpr float previewRoomMinX = -30.0f;
	constexpr float previewRoomMaxX = 30.0f;
	constexpr float previewRoomNearZ = -30.0f;
	constexpr float previewRoomFarZ = 38.0f;
	constexpr float previewRoomHeight = 32.0f;
	constexpr float previewGridStep = 4.0f;
	renderer.DrawTrainingRoomBackground(previewRoomMinX, previewRoomMaxX,
		previewRoomNearZ, previewRoomFarZ, previewRoomHeight, previewGridStep);

	const ModelResource* previewModel = ModelResourceManager::GetModel(PreviewModelKey);
	const std::vector<Matrix>* previewSkinningMatrices = nullptr;
	SkeletonPose ghostSkeletonPose;
	const std::vector<Matrix>* ghostSkinningMatrices = nullptr;
	if (previewModel && skeletonPose.initialized)
	{
		const MotionData* idleMotion = nullptr;
		SkeletonPose idleBasePose;
		const SkeletonPose* basePose = nullptr;
		if (MotionDataManager::LoadMotionData(CommonIdleMotionDataId))
		{
			idleMotion = MotionDataManager::GetMotionData(CommonIdleMotionDataId);
		}
		if (idleMotion)
		{
			MotionPose::ApplyMotionData(ghostSkeletonPose, *idleMotion, 0, *previewModel);
			MotionPose::UpdateSkinningMatrices(ghostSkeletonPose, *previewModel);
			ghostSkinningMatrices = &ghostSkeletonPose.skinningMatrices;
		}

		if (actionFrame < 0)
		{
			if (idleMotion)
			{
				MotionPose::ApplyMotionData(skeletonPose, *idleMotion, 0, *previewModel);
				MotionPose::UpdateSkinningMatrices(skeletonPose, *previewModel);
				previewSkinningMatrices = &skeletonPose.skinningMatrices;
			}
		}
		else if (!editingMotionDataId.empty())
		{
			const MotionData* motion = nullptr;
			if (hasDraftMotion && motionData.motionDataId == editingMotionDataId)
			{
				motion = &motionData;
			}
			else if (MotionDataManager::LoadMotionData(editingMotionDataId))
			{
				motion = MotionDataManager::GetMotionData(editingMotionDataId);
			}

			if (motion)
			{
				if (idleMotion && editingMotionDataId.rfind("Attack/", 0) == 0)
				{
					MotionPose::ApplyMotionData(idleBasePose, *idleMotion, 0, *previewModel);
					basePose = &idleBasePose;
				}

				MotionPose::ApplyMotionData(skeletonPose, *motion, actionFrame, *previewModel, basePose);
				MotionPose::UpdateSkinningMatrices(skeletonPose, *previewModel);
				previewSkinningMatrices = &skeletonPose.skinningMatrices;
			}
		}
	}
	lastRenderedActionFrame = previewSkinningMatrices ? actionFrame : -100000;

	if (region && previewModel && previewSkinningMatrices)
	{
		const ImVec2 origin = ImGui::GetMainViewport()->Pos;
		ImDrawList* overlay = ImGui::GetForegroundDrawList();
		// 3Dのビューポート制限はImGuiには伝わらないため、オーバーレイも明示的に切り取る。
		overlay->PushClipRect(ImVec2(origin.x + region->left, origin.y + region->top),
			ImVec2(origin.x + region->right, origin.y + region->bottom), true);
		DrawRotationGizmo(*region, *previewModel, selectedBodyPartIndex, rotationGizmoEnabled);
		overlay->PopClipRect();
	}

	if (previewModel && ghostSkinningMatrices)
	{
		TransformComponent ghostTransform = playerTransform;
		TransformSystem::SetLocalPosition(ghostTransform, GetPreviewPlayerBasePosition());
		TransformSystem::UpdateWorldTransform(ghostTransform);
		renderer.DrawModel(
			*previewModel,
			TransformSystem::GetWorldMatrix(ghostTransform),
			Color(0.65f, 0.85f, 1.0f, 0.22f),
			true,
			ghostSkinningMatrices);
	}

	const bool drewModel = previewModel
		&& renderer.DrawModel(*previewModel, TransformSystem::GetWorldMatrix(playerTransform), previewSkinningMatrices);
	if (!drewModel)
	{
		const Matrix fallbackWorld =
			Matrix::CreateScale(1.0f, 4.0f, 1.0f)
			* Matrix::CreateTranslation(Vector3(0.0f, 3.0f, 8.0f));
		renderer.DrawDebugCube(fallbackWorld);
	}

	DrawAttackBoxes(renderer, attackData, editingCommonMotion);
	if (!region)
	{
		Renderer::RestoreBackBuffer();
	}

	if (region && previewModel && previewSkinningMatrices)
	{
		const ImVec2 origin = ImGui::GetMainViewport()->Pos;
		ImDrawList* overlay = ImGui::GetForegroundDrawList();
		overlay->PushClipRect(ImVec2(origin.x + region->left, origin.y + region->top),
			ImVec2(origin.x + region->right, origin.y + region->bottom), true);
		DrawJointMarkers(*region, *previewModel, selectedBodyPartIndex);
		overlay->PopClipRect();
	}
}

bool CustomizePreviewController::ConsumeGizmoRotationEdit(
	int& outBodyPartIndex,
	int& outAxisIndex,
	Quaternion& outLocalRotation)
{
	if (!hasGizmoRotationEdit)
	{
		return false;
	}

	outBodyPartIndex = gizmoEditedBodyPartIndex;
	outAxisIndex = gizmoEditedAxisIndex;
	outLocalRotation = gizmoLocalRotation;
	hasGizmoRotationEdit = false;
	gizmoEditedBodyPartIndex = -1;
	gizmoEditedAxisIndex = -1;
	gizmoLocalRotation = Quaternion::Identity;
	return true;
}

bool CustomizePreviewController::BeginIkSolveSession(int actionFrame)
{
	if (!skeletonPose.initialized || !HasRenderedActionFrame(actionFrame))
	{
		hasIkSolveBasePose = false;
		return false;
	}

	// IK 数値編集中に毎フレーム MotionData へ回転を書き込むと、
	// 次フレームのプレビュー姿勢もその回転を含むため、そこからさらに IK を解くと回転が累積して暴れる。
	// 開始姿勢とは別に採用済み姿勢を保持し、下書きを読み戻さず連続した解を探索する。
	ikSolveBasePose = skeletonPose;
	ikSolveAcceptedPose = skeletonPose;
	ikSolveObjectWorld = TransformSystem::GetWorldMatrix(playerTransform);
	hasIkSolveBasePose = true;
	return true;
}

bool CustomizePreviewController::GetBodyPartBindRotation(int part, Quaternion& rotation) const
{
	const auto* model = ModelResourceManager::GetModel(PreviewModelKey);
	if (!model || part < 0 || part >= MotionBodyPartCount) return false;
	const int bone = MotionSkeleton::FindModelBoneIndex(*model, MotionSkeleton::GetBodyPartName(part));
	if (bone < 0) return false;
	rotation = model->GetBones()[bone].bindLocalRotation;
	return true;
}

void CustomizePreviewController::EndIkSolveSession()
{
	hasIkSolveBasePose = false;
	ikSolveBasePose = SkeletonPose{};
	ikSolveAcceptedPose = SkeletonPose{};
}

bool CustomizePreviewController::GetBodyPartWorldPosition(
	int bodyPartIndex,
	Vector3& outWorldPosition) const
{
	const ModelResource* previewModel = ModelResourceManager::GetModel(PreviewModelKey);
	if (!previewModel || !skeletonPose.initialized)
	{
		return false;
	}

	int modelBoneIndex = -1;
	Matrix bodyPartWorld;
	const Matrix playerWorld = TransformSystem::GetWorldMatrix(playerTransform);
	if (!GetBodyPartWorldMatrix(skeletonPose, *previewModel, playerWorld, bodyPartIndex, modelBoneIndex, bodyPartWorld))
	{
		return false;
	}

	outWorldPosition = Vector3::Transform(Vector3::Zero, bodyPartWorld);
	return true;
}

bool CustomizePreviewController::SolveIkTarget(
    int targetBodyPartIndex, const Vector3& targetWorldPosition, MotionIkResult& result)
{
    const ModelResource* model = ModelResourceManager::GetModel(PreviewModelKey);
    const auto* chain = MotionIk::FindChain(static_cast<MotionBodyPart>(targetBodyPartIndex));
    if (!model || !chain || !hasIkSolveBasePose) return false;
    // 入力値は開始時のワールド座標のまま。解の探索だけを前回の採用姿勢から続ける。
    // 下書きやプレビューから読み戻さないため、他の編集更新の混入を防ぐ。
    if (!MotionIk::Solve(*model, ikSolveAcceptedPose, ikSolveObjectWorld, *chain, targetWorldPosition, result)) return false;
    for (int i = 0; i < 3; ++i)
        ikSolveAcceptedPose.bonePoses[result.boneIndices[i]].localRotation = result.localRotations[i];
    MotionPose::UpdateSkinningMatrices(ikSolveAcceptedPose, *model);
    return true;
}

int CustomizePreviewController::ConsumePickedBodyPartIndex()
{
	const int result = pickedBodyPartIndex;
	pickedBodyPartIndex = -1;
	return result;
}

int CustomizePreviewController::GetCurrentFrame() const
{
	return currentFrame;
}

void CustomizePreviewController::SetCurrentFrame(int frame)
{
	currentFrame = frame;
}

int CustomizePreviewController::GetActionFrame() const
{
	return currentFrame - 1;
}

bool CustomizePreviewController::HasRenderedActionFrame(int actionFrame) const
{
	return lastRenderedActionFrame == actionFrame;
}

bool CustomizePreviewController::IsPlaying() const
{
	return playing;
}

const Renderer::RenderTexture& CustomizePreviewController::GetRenderTexture() const
{
	return renderTexture;
}

float& CustomizePreviewController::GetCameraYawDegrees()
{
	return cameraYawDegrees;
}

float& CustomizePreviewController::GetCameraPitchDegrees()
{
	return cameraPitchDegrees;
}

float& CustomizePreviewController::GetCameraDistance()
{
	return cameraDistance;
}

/// <summary>プレビュー内から始まったマウス操作だけを取り込み、カメラ状態を更新する。</summary>
/// <param name="region">ウィンドウ内のプレビュー領域。</param>
void CustomizePreviewController::UpdateCameraInput(const RECT& region)
{
	const ImGuiIO& io = ImGui::GetIO();
	const ImVec2 origin = ImGui::GetMainViewport()->Pos;
	const bool inside = io.MousePos.x >= origin.x + region.left && io.MousePos.x < origin.x + region.right
		&& io.MousePos.y >= origin.y + region.top && io.MousePos.y < origin.y + region.bottom;
	const Vector3 baseTarget = TransformSystem::GetLocalPosition(playerTransform) + Vector3(0, 4, 0);
	// 開始判定だけを領域内に限定。ドラッグ中は領域外でもボタン解除まで追跡する。
	if (cameraDragButton >= 0 && (!ImGui::IsMouseDown(cameraDragButton) || io.AppFocusLost)) cameraDragButton = -1;
	const bool editing = ImGuizmo::IsUsing() || rotationGizmoDragActive || translationGizmoActive || hasIkSolveBasePose;
	if (editing) { cameraDragButton = -1; return; }
	if (cameraDragButton < 0 && inside && !io.WantCaptureMouse && !ImGui::IsAnyItemActive())
	{
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) cameraDragButton = ImGuiMouseButton_Right;
		else if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) cameraDragButton = ImGuiMouseButton_Middle;
		if (cameraDragButton >= 0) cameraDragTarget = baseTarget + cameraTargetOffset;
	}
	if (cameraDragButton >= 0)
	{
		if (cameraDragButton == ImGuiMouseButton_Right)
		{
			cameraYawDegrees = std::remainder(cameraYawDegrees + io.MouseDelta.x * 0.3f, 360.0f);
			cameraPitchDegrees = std::clamp(cameraPitchDegrees - io.MouseDelta.y * 0.3f, -45.0f, 65.0f);
		}
		else
		{
			const float yaw = XMConvertToRadians(cameraYawDegrees), pitch = XMConvertToRadians(cameraPitchDegrees);
			const Vector3 right(std::cos(yaw), 0, -std::sin(yaw));
			const Vector3 forward(std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch));
			const Vector3 up = forward.Cross(right);
			// 距離と画角から1ピクセルのワールド移動量を求め、画面上の掴み心地を揃える。
			const float units = 2 * cameraDistance * std::tan(XMConvertToRadians(camera.fovYDegrees) * 0.5f)
				/ static_cast<float>(std::max(1L, region.bottom - region.top));
			cameraDragTarget += (-right * io.MouseDelta.x + up * io.MouseDelta.y) * units;
		}
		// 再生中にモデルが移動しても、操作中の注視点を勝手に追従させない。
		cameraTargetOffset = cameraDragTarget - baseTarget;
	}
	if (inside && !io.WantCaptureMouse && !ImGui::IsAnyItemActive() && io.MouseWheel != 0)
		cameraDistance = std::clamp(cameraDistance * std::pow(0.9f, io.MouseWheel), 5.0f, 30.0f);
}

/// <summary>注視点オフセットと旋回角度からカメラTransformを更新する。</summary>
void CustomizePreviewController::UpdateCameraTransform()
{
	cameraPitchDegrees = std::clamp(cameraPitchDegrees, -45.0f, 65.0f);
	cameraDistance = std::clamp(cameraDistance, 5.0f, 30.0f);

	const Vector3 targetPosition = TransformSystem::GetLocalPosition(playerTransform) + Vector3(0.0f, 4.0f, 0.0f) + cameraTargetOffset;
	const float yawRadians = XMConvertToRadians(cameraYawDegrees);
	const float pitchRadians = XMConvertToRadians(cameraPitchDegrees);
	const float cosPitch = std::cos(pitchRadians);
	const Vector3 forward(
		std::sin(yawRadians) * cosPitch,
		std::sin(pitchRadians),
		std::cos(yawRadians) * cosPitch);

	const Vector3 cameraPosition = targetPosition - forward * cameraDistance;
	TransformSystem::SetLocalPosition(cameraTransform, cameraPosition);
	// 位置の球面計算とEuler回転のPitch符号を別々に扱わない。
	// 実際の位置から注視点へのLookAtを作り、その逆行列からカメラの向きを復元する。
	Matrix view;
	XMStoreFloat4x4(&view, XMMatrixLookAtLH(XMLoadFloat3(&cameraPosition),
		XMLoadFloat3(&targetPosition), XMVectorSet(0, 1, 0, 0)));
	Quaternion rotation = Quaternion::CreateFromRotationMatrix(view.Invert());
	rotation.Normalize();
	TransformSystem::SetLocalRotationQuaternion(cameraTransform, rotation);
}

void CustomizePreviewController::DrawAttackBoxes(Renderer& renderer, const AttackData& attackData, bool editingCommonMotion)
{
	if (editingCommonMotion || !IsAttackFrameActive(attackData.frame, GetActionFrame()))
	{
		return;
	}

	const Color attackBoxColor(1.0f, 0.0f, 0.0f, 0.35f);
	const Vector3 basePosition = TransformSystem::GetWorldPosition(playerTransform);

	for (const AttackHitboxData& hitbox : attackData.hitboxes)
	{
		if (hitbox.size.x <= 0.0f || hitbox.size.y <= 0.0f)
		{
			continue;
		}

		const Vector3 center(
			basePosition.x + hitbox.offset.x,
			basePosition.y + hitbox.offset.y,
			basePosition.z);
		const Matrix boxWorld =
			Matrix::CreateScale(hitbox.size.x * 0.5f, hitbox.size.y * 0.5f, PreviewBoxDepth * 0.5f)
			* Matrix::CreateTranslation(center);

		renderer.DrawDebugBox(boxWorld, attackBoxColor);
	}
}

void CustomizePreviewController::DrawJointMarkers(
	const RECT& region,
	const ModelResource& model,
	int selectedBodyPartIndex)
{
	if (!skeletonPose.initialized || skeletonPose.boneWorldMatrices.empty())
	{
		return;
	}

	const ImVec2 viewportOrigin = ImGui::GetMainViewport()->Pos;
	const ImVec2 regionMin(
		viewportOrigin.x + static_cast<float>(region.left),
		viewportOrigin.y + static_cast<float>(region.top));
	const ImVec2 regionMax(
		viewportOrigin.x + static_cast<float>(region.right),
		viewportOrigin.y + static_cast<float>(region.bottom));
	const ImVec2 mousePosition = ImGui::GetIO().MousePos;
	const bool mouseInPreview =
		mousePosition.x >= regionMin.x
		&& mousePosition.x <= regionMax.x
		&& mousePosition.y >= regionMin.y
		&& mousePosition.y <= regionMax.y;

	std::array<ImVec2, MotionBodyPartCount> markerPositions = {};
	std::array<bool, MotionBodyPartCount> markerVisible = {};
	const Matrix playerWorld = TransformSystem::GetWorldMatrix(playerTransform);
	const Matrix view = camera.viewMatrix;
	const Matrix projection = camera.projectionMatrix;
	for (int bodyPartIndex = 0; bodyPartIndex < MotionBodyPartCount; ++bodyPartIndex)
	{
		const int modelBoneIndex = MotionSkeleton::FindModelBoneIndex(
			model,
			MotionSkeleton::GetBodyPartName(bodyPartIndex));
		if (modelBoneIndex < 0 || modelBoneIndex >= static_cast<int>(skeletonPose.boneWorldMatrices.size()))
		{
			continue;
		}

		const Vector3 modelPosition = Vector3::Transform(Vector3::Zero, skeletonPose.boneWorldMatrices[modelBoneIndex]);
		const Vector3 worldPosition = Vector3::Transform(modelPosition, playerWorld);
		ImVec2 screenPosition;
		if (!ProjectToPreviewScreen(worldPosition, view, projection, region, screenPosition))
		{
			continue;
		}

		markerPositions[bodyPartIndex] = screenPosition;
		markerVisible[bodyPartIndex] = true;
	}

	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	for (int bodyPartIndex = 0; bodyPartIndex < MotionBodyPartCount; ++bodyPartIndex)
	{
		if (!markerVisible[bodyPartIndex])
		{
			continue;
		}

		const MotionBodyPartDefinition& definition = MotionSkeleton::GetBodyPartDefinition(bodyPartIndex);
		const int parentIndex = static_cast<int>(definition.parent);
		if (parentIndex >= 0 && parentIndex < MotionBodyPartCount && markerVisible[parentIndex])
		{
			drawList->AddLine(
				markerPositions[parentIndex],
				markerPositions[bodyPartIndex],
				IM_COL32(80, 190, 255, 150),
				2.0f);
		}
	}

	int nearestBodyPartIndex = -1;
	float nearestDistanceSquared = JointPickRadius * JointPickRadius;
	for (int bodyPartIndex = 0; bodyPartIndex < MotionBodyPartCount; ++bodyPartIndex)
	{
		if (!markerVisible[bodyPartIndex])
		{
			continue;
		}

		const ImVec2 markerPosition = markerPositions[bodyPartIndex];
		const float dx = mousePosition.x - markerPosition.x;
		const float dy = mousePosition.y - markerPosition.y;
		const float distanceSquared = dx * dx + dy * dy;
		if (mouseInPreview && distanceSquared < nearestDistanceSquared)
		{
			nearestDistanceSquared = distanceSquared;
			nearestBodyPartIndex = bodyPartIndex;
		}
	}

	if (mouseInPreview && !ImGui::GetIO().WantCaptureMouse && cameraDragButton < 0
		&& ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		if (!ImGuizmo::IsOver() && !ImGuizmo::IsUsing())
		{
			// -1はイベントなし、-2は空白クリックによる明示的な選択解除。
			pickedBodyPartIndex = nearestBodyPartIndex >= 0 ? nearestBodyPartIndex : -2;
		}
	}

	for (int bodyPartIndex = 0; bodyPartIndex < MotionBodyPartCount; ++bodyPartIndex)
	{
		if (!markerVisible[bodyPartIndex])
		{
			continue;
		}

		const bool selected = bodyPartIndex == selectedBodyPartIndex;
		const bool hovered = bodyPartIndex == nearestBodyPartIndex;
		const float radius = selected ? JointMarkerRadius + 3.0f : hovered ? JointMarkerRadius + 2.0f : JointMarkerRadius;
		const ImU32 fillColor = selected
			? IM_COL32(255, 230, 40, 245)
			: hovered
				? IM_COL32(255, 245, 120, 235)
				: IM_COL32(255, 220, 20, 205);
		drawList->AddCircleFilled(markerPositions[bodyPartIndex], radius, fillColor, 18);
		drawList->AddCircle(markerPositions[bodyPartIndex], radius, IM_COL32(0, 35, 55, 230), 18, 2.0f);
		if (selected || hovered)
		{
			drawList->AddText(
				ImVec2(markerPositions[bodyPartIndex].x + 10.0f, markerPositions[bodyPartIndex].y - 10.0f),
				IM_COL32(255, 255, 255, 245),
				MotionSkeleton::GetBodyPartName(bodyPartIndex));
		}
	}
}

void CustomizePreviewController::DrawRotationGizmo(
	const RECT& region,
	const ModelResource& model,
	int selectedBodyPartIndex,
	bool enabled)
{
	if (!enabled || !skeletonPose.initialized || skeletonPose.boneWorldMatrices.empty())
	{
		rotationGizmoDragActive = false;
		if (translationGizmoActive) EndIkSolveSession();
		translationGizmoActive = false;
		return;
	}
	if (selectedBodyPartIndex < 0 || selectedBodyPartIndex >= MotionBodyPartCount)
	{
		rotationGizmoDragActive = false;
		if (translationGizmoActive) EndIkSolveSession();
		translationGizmoActive = false;
		return;
	}

	const int modelBoneIndex = MotionSkeleton::FindModelBoneIndex(
		model,
		MotionSkeleton::GetBodyPartName(selectedBodyPartIndex));
	if (modelBoneIndex < 0 || modelBoneIndex >= static_cast<int>(skeletonPose.boneWorldMatrices.size()))
	{
		rotationGizmoDragActive = false;
		return;
	}

	Matrix boneWorld;
	const Matrix playerWorld = TransformSystem::GetWorldMatrix(playerTransform);
	if (!MotionPose::GetBoneWorldMatrix(skeletonPose, modelBoneIndex, playerWorld, boneWorld))
	{
		rotationGizmoDragActive = false;
		return;
	}

	const ImVec2 viewportOrigin = ImGui::GetMainViewport()->Pos;
	const float regionX = viewportOrigin.x + static_cast<float>(region.left);
	const float regionY = viewportOrigin.y + static_cast<float>(region.top);
	const float regionWidth = static_cast<float>(region.right - region.left);
	const float regionHeight = static_cast<float>(region.bottom - region.top);
	if (regionWidth <= 0.0f || regionHeight <= 0.0f)
	{
		return;
	}

	ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetRect(regionX, regionY, regionWidth, regionHeight);
	ImGuizmo::SetGizmoSizeClipSpace(RotationGizmoSizeClipSpace);
	if (translationGizmoEnabled)
	{
		rotationGizmoDragActive = false;
		const auto* chain = MotionIk::FindChain(static_cast<MotionBodyPart>(selectedBodyPartIndex));
		if (!chain || chain->translationLocked) return;
		if (translationGizmoActive && (translationGizmoPart != selectedBodyPartIndex || translationGizmoFrame != GetActionFrame()))
		{
			translationGizmoActive = false;
			EndIkSolveSession();
		}
		// 要求位置の行列は到達位置で上書きしない。WORLDで関節回転に依存しない矢印を使う。
		if (!translationGizmoActive) translationGizmoMatrix = Matrix::CreateTranslation(boneWorld.Translation());
		ImGuizmo::SetRotationAngleLimit(false, 0, 0);
		ImGuizmo::SetTranslationAxesOnly(true);
		const bool changed = ImGuizmo::Manipulate(&camera.viewMatrix._11, &camera.projectionMatrix._11,
			ImGuizmo::TRANSLATE, ImGuizmo::WORLD, &translationGizmoMatrix._11);
		ImGuizmo::SetTranslationAxesOnly(false);
		const bool usingGizmo = ImGuizmo::IsUsing();
		const int handle = ImGuizmo::GetActiveHandleType();
		const bool axisHandle = handle == ImGuizmo::MT_MOVE_X || handle == ImGuizmo::MT_MOVE_Y || handle == ImGuizmo::MT_MOVE_Z;
		if (usingGizmo && !translationGizmoActive && axisHandle)
		{
			Stop();
			translationGizmoActive = BeginIkSolveSession(GetActionFrame());
			translationGizmoPart = selectedBodyPartIndex;
			translationGizmoFrame = GetActionFrame();
		}
		if (changed && translationGizmoActive && axisHandle
			&& SolveIkTarget(selectedBodyPartIndex, translationGizmoMatrix.Translation(), translationGizmoResult))
		{
			hasTranslationGizmoEdit = true;
			for (int i = 0; i < 3; ++i)
				skeletonPose.bonePoses[translationGizmoResult.boneIndices[i]].localRotation = translationGizmoResult.localRotations[i];
			MotionPose::UpdateSkinningMatrices(skeletonPose, model);
		}
		if (!usingGizmo && translationGizmoActive)
		{
			translationGizmoActive = false;
			EndIkSolveSession();
		}
		return;
	}

	if (rotationGizmoDragActive && rotationGizmoDragBodyPartIndex != selectedBodyPartIndex)
	{
		rotationGizmoDragActive = false;
	}

	const bool wasDragActive = rotationGizmoDragActive;
	Matrix editableWorld = rotationGizmoDragActive ? rotationGizmoDragWorldMatrix : boneWorld;
	ImGuizmo::SetRotationAngleLimit(false, 0.0f, 0.0f);
	if (rotationGizmoDragActive)
	{
		const auto part = static_cast<MotionBodyPart>(selectedBodyPartIndex);
		if (MotionSkeleton::GetPoseRotationLimit(part).enabled)
		{
			ImGuizmo::SetRotationAngleLimit(true, rotationGizmoDragLimits.x, rotationGizmoDragLimits.y);
		}
	}

	const ImGuizmo::OPERATION rotationOperation =
		ImGuizmo::ROTATE_X | ImGuizmo::ROTATE_Y | ImGuizmo::ROTATE_Z;
	const bool manipulated = ImGuizmo::Manipulate(
		&camera.viewMatrix._11,
		&camera.projectionMatrix._11,
		rotationOperation,
		ImGuizmo::LOCAL,
		&editableWorld._11);

	const bool usingGizmo = ImGuizmo::IsUsing();
	int axisIndex = GetRotationAxisIndex(ImGuizmo::GetActiveHandleType());
	if (axisIndex < 0 && rotationGizmoDragActive && manipulated)
	{
		axisIndex = rotationGizmoDragAxisIndex;
	}
	if (axisIndex < 0 && rotationGizmoDragActive)
	{
		axisIndex = rotationGizmoDragAxisIndex;
	}
	if (!usingGizmo && !wasDragActive)
	{
		rotationGizmoDragActive = false;
		return;
	}
	if (axisIndex < 0)
	{
		if (!usingGizmo)
		{
			rotationGizmoDragActive = false;
		}
		return;
	}

	const bool initializeRange = !rotationGizmoDragActive || rotationGizmoDragAxisIndex != axisIndex;
	if (!rotationGizmoDragActive)
	{
		rotationGizmoDragActive = true;
		rotationGizmoDragBodyPartIndex = selectedBodyPartIndex;
		rotationGizmoDragAxisIndex = axisIndex;
		rotationGizmoDragWorldMatrix = boneWorld;
		rotationGizmoDragStartLocalRotation = skeletonPose.bonePoses[modelBoneIndex].localRotation;
		rotationGizmoDragStartLocalRotation.Normalize();
		rotationGizmoDragAccumulatedRadians = 0.0f;
	}
	if (rotationGizmoDragAxisIndex != axisIndex)
	{
		rotationGizmoDragAxisIndex = axisIndex;
		rotationGizmoDragWorldMatrix = boneWorld;
		rotationGizmoDragStartLocalRotation = skeletonPose.bonePoses[modelBoneIndex].localRotation;
		rotationGizmoDragStartLocalRotation.Normalize();
		rotationGizmoDragAccumulatedRadians = 0.0f;
	}

	const float rotationDeltaRadians = ImGuizmo::GetRotationLastDeltaRadians();
	rotationGizmoDragAccumulatedRadians += rotationDeltaRadians;
	const Vector3 localAxis = GetLocalAxisByIndex(rotationGizmoDragAxisIndex);
	if (localAxis.LengthSquared() <= std::numeric_limits<float>::epsilon())
	{
		return;
	}

	const auto part = static_cast<MotionBodyPart>(selectedBodyPartIndex);
	if (MotionSkeleton::GetPoseRotationLimit(part).enabled)
	{
		if (initializeRange)
			rotationGizmoDragLimits = MotionRotationLimits::AxisRange(part, rotationGizmoDragStartLocalRotation,
				localAxis, model.GetBones()[modelBoneIndex].bindLocalRotation);
		rotationGizmoDragAccumulatedRadians = std::clamp(rotationGizmoDragAccumulatedRadians, rotationGizmoDragLimits.x, rotationGizmoDragLimits.y);
	}
	// 掴んだ瞬間は差分0でも、次フレームで使う可動域は先に確定する。
	// ここより前で戻ると開始フラグだけが立ち、未初期化の範囲(0,0)や
	// 前のドラッグの範囲が、その後の操作に使われ続けてしまう。
	if (!wasDragActive && !manipulated && std::abs(rotationDeltaRadians) <= std::numeric_limits<float>::epsilon())
	{
		return;
	}
	Quaternion deltaRotation = Quaternion::CreateFromAxisAngle(localAxis, rotationGizmoDragAccumulatedRadians);
	deltaRotation.Normalize();
	Quaternion editedLocalRotation = deltaRotation * rotationGizmoDragStartLocalRotation;
	editedLocalRotation.Normalize();

	// ドラッグ中は MotionData を書き換えず、プレビュー用 SkeletonPose だけを Quaternion で直接更新する。
	// 制限範囲・途中姿勢・確定保存までQuaternionを使い、Eulerへ往復しない。
	skeletonPose.bonePoses[modelBoneIndex].localRotation = editedLocalRotation;
	MotionPose::UpdateSkinningMatrices(skeletonPose, model);
	if (!usingGizmo)
	{
		hasGizmoRotationEdit = true;
		gizmoEditedBodyPartIndex = selectedBodyPartIndex;
		gizmoEditedAxisIndex = rotationGizmoDragAxisIndex;
		gizmoLocalRotation = editedLocalRotation;
		rotationGizmoDragActive = false;
	}
}
