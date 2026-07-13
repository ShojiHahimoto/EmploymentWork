#include "Scene/BattleScene.h"

#include "Component/ModelComponent.h"
#include "Resource/ModelResource.h"
#include "System/CameraSystem.h"
#include "System/DebugCameraControlSystem.h"
#include "System/DebugImGuiSystem.h"
#include "System/Debugger.h"
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

	ModelResourceManager::LoadModel(
		"Debugman",
		"assets/model/Debugman/Akai.fbx",
		Renderer::GetDevice());

	world.RequestSpawn(
		SpawnType::Debugman,
		"Debugman",
		Vector3(-2.0f, -1.0f, 8.0f),
		Vector3(0.0f, 180.0f, 0.0f));

	world.RequestSpawn(
		SpawnType::DebugCube,
		"DebugCube",
		Vector3(0.0f, 0.0f, 6.0f),
		Vector3(20.0f, 32.0f, 0.0f));

	CameraComponent camera;
	const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
	CameraSystem::SetPerspective(camera, 45.0f, aspectRatio, 0.1f, 1000.0f);
	world.SetActiveCamera(cameraId, camera);

#if defined(_DEBUG)
	InitializeDebugSceneView();
#endif

	RunSystems();
}

void BattleScene::Exit()
{
#if defined(_DEBUG)
	Renderer::ReleaseRenderTexture(sceneViewRenderTexture);
#endif
	world.Clear();
}

void BattleScene::RunSystems()
{
#if defined(_DEBUG)
	UpdateDebugSceneViewCamera();
#endif

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
	DrawWorldWithCamera(renderer, camera);

#if defined(_DEBUG)
	DrawDebugSceneView(renderer);
#endif

	DebugImGuiSystem::DrawSpawnWindow(world);
	DebugImGuiSystem::DrawWorldInspector(world);
}

void BattleScene::DrawWorldWithCamera(Renderer& renderer, const CameraComponent& camera)
{
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
			const ModelComponent* modelComponent = world.GetComponent<ModelComponent>(object.id);
			if (modelComponent)
			{
				const ModelResource* model = ModelResourceManager::GetModel(modelComponent->resourceKey);
				if (model)
				{
					if (renderer.DrawModel(*model, TransformSystem::GetWorldMatrix(*transform)))
					{
						continue;
					}
				}
			}

			renderer.DrawDebugCube(TransformSystem::GetWorldMatrix(*transform));
		}
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

#if defined(_DEBUG)
void BattleScene::InitializeDebugSceneView()
{
	debugSceneCameraTransform = TransformComponent{};
	debugSceneCamera = CameraComponent{};
	debugSceneCameraControlState = DebugCameraControlState{};

	TransformSystem::SetLocalPosition(debugSceneCameraTransform, Vector3(0.0f, 2.0f, -8.0f));
	TransformSystem::SetLocalEulerRotationDegrees(debugSceneCameraTransform, Vector3(10.0f, 0.0f, 0.0f));
	TransformSystem::SetLocalScale(debugSceneCameraTransform, Vector3::One);
	TransformSystem::UpdateWorldTransform(debugSceneCameraTransform);

	const float aspectRatio = static_cast<float>(sceneViewWidth) / static_cast<float>(sceneViewHeight);
	CameraSystem::SetPerspective(debugSceneCamera, 45.0f, aspectRatio, 0.1f, 1000.0f);
	CameraSystem::Update(debugSceneCamera, debugSceneCameraTransform);

	const HRESULT hr = Renderer::CreateRenderTexture(sceneViewRenderTexture, sceneViewWidth, sceneViewHeight);
	if (FAILED(hr))
	{
		DebugLog("[SceneView] RenderTexture creation failed. hr=", static_cast<long>(hr));
	}
}

void BattleScene::UpdateDebugSceneViewCamera()
{
	DebugCameraControlSystem::Update(
		debugSceneCameraTransform,
		debugSceneCameraControlState,
		sceneViewHovered);

	TransformSystem::UpdateWorldTransform(debugSceneCameraTransform);
	CameraSystem::Update(debugSceneCamera, debugSceneCameraTransform);
}

void BattleScene::DrawDebugSceneView(Renderer& renderer)
{
	if (!sceneViewRenderTexture.shaderResourceView)
	{
		sceneViewHovered = false;
		return;
	}

	const float clearColor[4] = { 0.05f, 0.05f, 0.08f, 1.0f };
	Renderer::BeginRenderTexture(sceneViewRenderTexture, clearColor);
	DrawWorldWithCamera(renderer, debugSceneCamera);
	Renderer::RestoreBackBuffer();

	sceneViewHovered = DebugImGuiSystem::DrawSceneView(
		sceneViewRenderTexture.shaderResourceView,
		sceneViewRenderTexture.width,
		sceneViewRenderTexture.height);
}
#endif
