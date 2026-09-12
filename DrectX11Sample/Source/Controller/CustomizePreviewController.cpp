#include "Controller/CustomizePreviewController.h"

#include "Data/MotionDataLoader.h"
#include "Resource/ModelResource.h"
#include "System/CameraSystem.h"
#include "System/Debugger.h"
#include "System/MotionPose.h"
#include "System/TransformSystem.h"
#include "System/imgui-docking/imgui.h"

#include <DirectXMath.h>

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
	Renderer::ReleaseRenderTexture(renderTexture);
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
	int selectedBodyPartIndex)
{
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
		regionScope.emplace(*region);
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

	UpdateCameraTransform();
	TransformSystem::UpdateWorldTransform(playerTransform);
	TransformSystem::UpdateWorldTransform(cameraTransform);
	CameraSystem::Update(camera, cameraTransform);
	renderer.SetViewProjection(camera.viewMatrix, camera.projectionMatrix);

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
		DrawJointMarkers(*region, *previewModel, selectedBodyPartIndex);
	}
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

void CustomizePreviewController::UpdateCameraTransform()
{
	cameraPitchDegrees = std::clamp(cameraPitchDegrees, -45.0f, 65.0f);
	cameraDistance = std::clamp(cameraDistance, 5.0f, 30.0f);

	const Vector3 targetPosition = TransformSystem::GetLocalPosition(playerTransform) + Vector3(0.0f, 4.0f, 0.0f);
	const float yawRadians = XMConvertToRadians(cameraYawDegrees);
	const float pitchRadians = XMConvertToRadians(cameraPitchDegrees);
	const float cosPitch = std::cos(pitchRadians);
	const Vector3 forward(
		std::sin(yawRadians) * cosPitch,
		std::sin(pitchRadians),
		std::cos(yawRadians) * cosPitch);

	const Vector3 cameraPosition = targetPosition - forward * cameraDistance;
	TransformSystem::SetLocalPosition(cameraTransform, cameraPosition);
	TransformSystem::SetLocalEulerRotationDegrees(
		cameraTransform,
		Vector3(cameraPitchDegrees, cameraYawDegrees, 0.0f));
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

	if (mouseInPreview && nearestBodyPartIndex >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		pickedBodyPartIndex = nearestBodyPartIndex;
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
