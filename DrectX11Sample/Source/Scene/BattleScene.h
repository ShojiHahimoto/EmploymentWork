#pragma once

#include "Scene/Scene.h"
#include "Component/CameraComponent.h"
#include "Component/TransformComponent.h"
#include "System/Renderer.h"
#include "System/DebugCameraControlSystem.h"
#include "World/World.h"

class BattleScene : public Scene
{
public:
	BattleScene(int initialWidth, int initialHeight);
	~BattleScene() override = default;

	void Enter() override;
	void Exit() override;
	void RunSystems() override;
	void Draw(Renderer& renderer) override;
	void OnResize(int width, int height) override;

	World& GetWorld() override;
	const World& GetWorld() const override;

private:
	World world;
	int width;
	int height;

	void DrawWorldWithCamera(Renderer& renderer, const CameraComponent& camera);

#if defined(_DEBUG)
	TransformComponent debugSceneCameraTransform;
	CameraComponent debugSceneCamera;
	DebugCameraControlState debugSceneCameraControlState;
	Renderer::RenderTexture sceneViewRenderTexture;
	bool sceneViewHovered = false;
	int sceneViewWidth = 640;
	int sceneViewHeight = 360;

	void InitializeDebugSceneView();
	void UpdateDebugSceneViewCamera();
	void DrawDebugSceneView(Renderer& renderer);
#endif
};
