#pragma once

#include "Scene/Scene.h"
#include "Component/CameraComponent.h"
#include "Component/TransformComponent.h"
#include "Data/BattleSetupData.h"
#include "System/Renderer.h"
#include "System/DebugCameraControlSystem.h"
#include "World/World.h"

class BattleScene : public Scene
{
public:
	BattleScene(int initialWidth, int initialHeight);
	BattleScene(int initialWidth, int initialHeight, const BattleSetup::BattleSetupData& battleSetupData);
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
	BattleSetup::BattleSetupData setupData;
	int width;
	int height;
	ID3D11ShaderResourceView* hudNumberTexture = nullptr;

	void InitializeBattleHUD();
	void RunInitialWorldSetup();
	void DrawWorldWithCamera(Renderer& renderer, const CameraComponent& camera);
	void DrawBattleHUD();
	void RequestResultSceneIfBattleFinished();

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
	void DrawDebugHitBoxes(Renderer& renderer);
#endif
};
