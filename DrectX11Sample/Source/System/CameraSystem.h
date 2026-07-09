#pragma once

#include "Component/CameraComponent.h"
#include "Component/TransformComponent.h"

class CameraSystem
{
public:
	// DirectX 左手座標系に合わせ、カメラの前方向は +Z とする。
	static constexpr DirectX::SimpleMath::Vector3 LocalForward = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 1.0f);
	static constexpr DirectX::SimpleMath::Vector3 LocalUp = DirectX::SimpleMath::Vector3(0.0f, 1.0f, 0.0f);

	static void SetPerspective(
		CameraComponent& camera,
		float fovYDegrees,
		float aspectRatio,
		float nearClip,
		float farClip);

	static void SetAspectRatio(CameraComponent& camera, float aspectRatio);

	// TransformSystem::UpdateWorldTransforms 後の Transform を使って View / Projection を更新する。
	static void Update(CameraComponent& camera, const TransformComponent& cameraTransform);

private:
	static DirectX::SimpleMath::Matrix CreateViewMatrix(const TransformComponent& cameraTransform);
	static DirectX::SimpleMath::Matrix CreateProjectionMatrix(const CameraComponent& camera);
};
