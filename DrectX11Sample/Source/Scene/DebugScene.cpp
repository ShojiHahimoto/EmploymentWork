#include "Scene/DebugScene.h"
#include "System/CameraSystem.h"
#include "System/DebugCameraControlSystem.h"
#include "System/Renderer.h"

using namespace DirectX::SimpleMath;

DebugScene::DebugScene(int initialWidth, int initialHeight)
	: width(initialWidth)
	, height(initialHeight)
{
}

void DebugScene::Enter()
{
	GameObjectId cameraId = world.CreateTransform();
	TransformComponent* cameraTransform = world.GetTransform(cameraId);
	if (cameraTransform)
	{
		TransformSystem::SetLocalPosition(*cameraTransform, Vector3(0.0f, 0.0f, -6.0f));
		TransformSystem::SetLocalEulerRotationDegrees(*cameraTransform, Vector3(0.0f, 0.0f, 0.0f));
	}

	debugCameraControlState = DebugCameraControlState{};

	CameraComponent camera;
	const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
	CameraSystem::SetPerspective(camera, 45.0f, aspectRatio, 0.1f, 1000.0f);
	world.SetActiveCamera(cameraId, camera);

	RunSystems();
}

void DebugScene::Exit()
{
	world.Clear();
}

void DebugScene::RunSystems()
{
	if (world.HasActiveCamera())
	{
		TransformComponent* cameraTransform = world.GetTransform(world.GetActiveCameraId());
		if (cameraTransform)
		{
			DebugCameraControlSystem::Update(*cameraTransform, debugCameraControlState);
		}
	}

	TransformSystem::UpdateWorldTransforms(world.GetTransforms());

	if (world.HasActiveCamera())
	{
		CameraComponent& camera = world.GetActiveCamera();
		TransformComponent* cameraTransform = world.GetTransform(world.GetActiveCameraId());
		if (cameraTransform)
		{
			CameraSystem::Update(camera, *cameraTransform);
		}
	}
}

void DebugScene::Draw(Renderer& renderer)
{
	if (!world.HasActiveCamera())
	{
		return;
	}

	const CameraComponent& camera = world.GetActiveCamera();
	renderer.SetViewProjection(camera.viewMatrix, camera.projectionMatrix);
	renderer.DrawDebugCube(
		Matrix::CreateRotationX(0.35f) *
		Matrix::CreateRotationY(0.55f));
}

void DebugScene::OnResize(int newWidth, int newHeight)
{
	if (newWidth <= 0 || newHeight <= 0)
	{
		return;
	}

	width = newWidth;
	height = newHeight;

	if (world.HasActiveCamera())
	{
		const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
		CameraSystem::SetAspectRatio(world.GetActiveCamera(), aspectRatio);
		RunSystems();
	}
}

World& DebugScene::GetWorld()
{
	return world;
}

const World& DebugScene::GetWorld() const
{
	return world;
}
