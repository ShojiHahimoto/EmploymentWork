#include "Scene/BattleScene.h"

#include "System/CameraSystem.h"
#include "System/DebugCameraControlSystem.h"
#include "System/DebugImGuiSystem.h"
#include "System/Renderer.h"
#include "System/SpawnDestroySystem.h"
#include "System/TransformSystem.h"

using namespace DirectX::SimpleMath;

BattleScene::BattleScene(int initialWidth, int initialHeight)
	: width(initialWidth)
	, height(initialHeight)
{
}

void BattleScene::Enter()
{
	GameObjectId cameraId = world.CreateTransform("MainCamera");
	TransformComponent* cameraTransform = world.GetTransform(cameraId);
	if (cameraTransform)
	{
		TransformSystem::SetLocalPosition(*cameraTransform, Vector3(0.0f, 0.0f, 0.0f));
		TransformSystem::SetLocalEulerRotationDegrees(*cameraTransform, Vector3(0.0f, 0.0f, 0.0f));
	}

	debugCameraControlState = DebugCameraControlState{};

	world.RequestSpawn(
		SpawnType::DebugCube,
		"DebugCube",
		Vector3(0.0f, 0.0f, 6.0f),
		Vector3(20.0f, 32.0f, 0.0f));

	CameraComponent camera;
	const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
	CameraSystem::SetPerspective(camera, 45.0f, aspectRatio, 0.1f, 1000.0f);
	world.SetActiveCamera(cameraId, camera);

	RunSystems();
}

void BattleScene::Exit()
{
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

	SpawnDestroySystem::Update(world);
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

	for (GameObject& object : world.GetGameObjects())
	{
		if (object.id == world.GetActiveCameraId())
		{
			continue;
		}

		TransformComponent* transform = world.GetTransform(object.id);
		if (transform)
		{
			renderer.DrawDebugCube(TransformSystem::GetWorldMatrix(*transform));
		}
	}

	DebugImGuiSystem::DrawSpawnWindow(world);
	DebugImGuiSystem::DrawWorldInspector(world);
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
