#pragma once

#include "Component/CameraComponent.h"
#include "Core/GameObjectId.h"
#include "System/TransformSystem.h"

class World
{
public:
	GameObjectId CreateTransform();
	void Clear();

	TransformSystem::TransformTable& GetTransforms();
	const TransformSystem::TransformTable& GetTransforms() const;

	TransformComponent* GetTransform(GameObjectId objectId);
	const TransformComponent* GetTransform(GameObjectId objectId) const;

	void SetActiveCamera(GameObjectId cameraId, const CameraComponent& camera);
	GameObjectId GetActiveCameraId() const;
	CameraComponent& GetActiveCamera();
	const CameraComponent& GetActiveCamera() const;
	bool HasActiveCamera() const;

private:
	TransformSystem::TransformTable transforms;
	GameObjectId nextObjectId = 1;

	GameObjectId activeCameraId = INVALID_GAME_OBJECT_ID;
	CameraComponent activeCamera;
};
