#include "Scene/BattleScene.h"

#include "System/CameraSystem.h"
#include "System/DebugCameraControlSystem.h"
#include "System/DebugImGuiSystem.h"
#include "System/Renderer.h"
#include "System/TransformSystem.h"

using namespace DirectX::SimpleMath;

BattleScene::BattleScene(int initialWidth, int initialHeight)
	: width(initialWidth)
	, height(initialHeight)
{
}

void BattleScene::Enter()
{
	GameObjectId cameraId = world.CreateTransform();
	TransformComponent* cameraTransform = world.GetTransform(cameraId);
	if (cameraTransform)
	{
		TransformSystem::SetLocalPosition(*cameraTransform, Vector3(0.0f, 0.0f, 0.0f));
		TransformSystem::SetLocalEulerRotationDegrees(*cameraTransform, Vector3(0.0f, 0.0f, 0.0f));
	}

	debugCameraControlState = DebugCameraControlState{};

	// デバッグ用キューブ配置
	debugCubeId = world.CreateTransform();
	TransformComponent* debugCubeTransform = world.GetTransform(debugCubeId);
	if (debugCubeTransform)
	{
		TransformSystem::SetLocalPosition(*debugCubeTransform, Vector3(0.0f, 0.0f, 6.0f));
		TransformSystem::SetLocalEulerRotationDegrees(*debugCubeTransform, Vector3(20.0f, 32.0f, 0.0f));
		TransformSystem::SetLocalScale(*debugCubeTransform, Vector3::One);
	}

	CameraComponent camera;
	const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
	CameraSystem::SetPerspective(camera, 45.0f, aspectRatio, 0.1f, 1000.0f);
	world.SetActiveCamera(cameraId, camera);

	RunSystems();
}

void BattleScene::Exit()
{
	debugCubeId = INVALID_GAME_OBJECT_ID;
	world.Clear();
}

void BattleScene::RunSystems()
{
	if (world.HasActiveCamera())
	{
		TransformComponent* cameraTransform = world.GetTransform(world.GetActiveCameraId());
		if (cameraTransform)
		{
			DebugCameraControlSystem::Update(*cameraTransform, debugCameraControlState);
		}
	}

	TransformSystem::UpdateWorldTransforms(world.GetGameObjects());

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

void BattleScene::Draw(Renderer& renderer)
{
	if (!world.HasActiveCamera())
	{
		return;
	}

	const CameraComponent& camera = world.GetActiveCamera();
	renderer.SetViewProjection(camera.viewMatrix, camera.projectionMatrix);

	TransformComponent* debugCubeTransform = world.GetTransform(debugCubeId);
	if (debugCubeTransform)
	{
		renderer.DrawDebugCube(TransformSystem::GetWorldMatrix(*debugCubeTransform));
		DebugImGuiSystem::DrawTransformEditor("Battle Debug Cube Transform", *debugCubeTransform);
	}
}

void BattleScene::OnResize(int newWidth, int newHeight)
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

World& BattleScene::GetWorld()
{
	return world;
}

const World& BattleScene::GetWorld() const
{
	return world;
}
