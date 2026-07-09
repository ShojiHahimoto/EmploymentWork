#include "System/CameraSystem.h"

#include <DirectXMath.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;

namespace
{
	float ClampMin(float value, float minValue)
	{
		return (value < minValue) ? minValue : value;
	}
}

void CameraSystem::SetPerspective(
	CameraComponent& camera,
	float fovYDegrees,
	float aspectRatio,
	float nearClip,
	float farClip)
{
	camera.fovYDegrees = ClampMin(fovYDegrees, 1.0f);
	camera.aspectRatio = ClampMin(aspectRatio, 0.001f);
	camera.nearClip = ClampMin(nearClip, 0.001f);
	camera.farClip = ClampMin(farClip, camera.nearClip + 0.001f);
	camera.projectionDirty = true;
}

void CameraSystem::SetAspectRatio(CameraComponent& camera, float aspectRatio)
{
	camera.aspectRatio = ClampMin(aspectRatio, 0.001f);
	camera.projectionDirty = true;
}

void CameraSystem::Update(CameraComponent& camera, const TransformComponent& cameraTransform)
{
	camera.viewMatrix = CreateViewMatrix(cameraTransform);

	if (camera.projectionDirty)
	{
		camera.projectionMatrix = CreateProjectionMatrix(camera);
		camera.projectionDirty = false;
	}

	camera.viewProjectionMatrix = camera.viewMatrix * camera.projectionMatrix;
}

Matrix CameraSystem::CreateViewMatrix(const TransformComponent& cameraTransform)
{
	const Vector3 eye = cameraTransform.worldPosition;

	const XMVECTOR rotation = XMLoadFloat4(&cameraTransform.worldRotation);
	const XMVECTOR forward = XMVector3Normalize(XMVector3Rotate(XMLoadFloat3(&LocalForward), rotation));
	const XMVECTOR up = XMVector3Normalize(XMVector3Rotate(XMLoadFloat3(&LocalUp), rotation));
	const XMVECTOR eyeVector = XMLoadFloat3(&eye);

	Matrix view;
	XMStoreFloat4x4(&view, XMMatrixLookToLH(eyeVector, forward, up));

	return view;
}

Matrix CameraSystem::CreateProjectionMatrix(const CameraComponent& camera)
{
	const float fovRadians = XMConvertToRadians(camera.fovYDegrees);

	Matrix projection;
	XMStoreFloat4x4(
		&projection,
		XMMatrixPerspectiveFovLH(
			fovRadians,
			camera.aspectRatio,
			camera.nearClip,
			camera.farClip));

	return projection;
}
