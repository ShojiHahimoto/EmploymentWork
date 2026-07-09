#pragma once

#include <SimpleMath.h>

struct CameraComponent
{
	// カメラ固有の投影設定。
	// 位置と回転は TransformComponent 側で管理する。
	float fovYDegrees = 45.0f;
	float aspectRatio = 16.0f / 9.0f;
	float nearClip = 0.1f;
	float farClip = 1000.0f;

	// CameraSystem が毎フレーム更新する行列キャッシュ。
	DirectX::SimpleMath::Matrix viewMatrix = DirectX::SimpleMath::Matrix::Identity;
	DirectX::SimpleMath::Matrix projectionMatrix = DirectX::SimpleMath::Matrix::Identity;
	DirectX::SimpleMath::Matrix viewProjectionMatrix = DirectX::SimpleMath::Matrix::Identity;

	// 投影設定が変わり、Projection の再計算が必要な状態。
	bool projectionDirty = true;
};
