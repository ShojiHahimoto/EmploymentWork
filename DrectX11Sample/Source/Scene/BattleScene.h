#pragma once

#include "Scene/Scene.h"
#include "Component/CameraComponent.h"
#include "Component/TransformComponent.h"
#include "Data/BattleSetupData.h"
#include "Core/Renderer.h"
#include "System/DebugCameraControlSystem.h"
#include "Core/World.h"
#include "System/BattleResultSystem.h"

class BattleScene : public Scene
{
public:
	BattleScene(int initialWidth, int initialHeight);
	BattleScene(int initialWidth, int initialHeight, const BattleSetup::BattleSetupData& battleSetupData);
	~BattleScene() override = default;

	void Enter() override;
	void DescribeLoad(SceneLoadRequest &request) const override;
	void Exit() override;
	void Update() override;
	void Draw(Renderer& renderer) override;
	void OnResize(int width, int height) override;

	World& GetWorld() override;
	const World& GetWorld() const override;

private:
	World world;
	BattleRoundState round;
	BattleResultMenu resultMenu;
	std::array<int, 2> visibleRoundPoints{};
	BattleRoundTiming roundTiming;
	int introFrame = 0;
	int endingFrame = 0;
	int fadeFrame = -1;
	bool postStopMotionStarted = false;
	bool victoryMotionStarted = false;
	const char* roundBanner = nullptr;
	BattleSetup::BattleSetupData setupData;
	int width;
	int height;
	ID3D11ShaderResourceView* hudNumberTexture = nullptr;

	void InitializeBattleHUD();
	void InitializeBattleSounds();
	void RunInitialWorldSetup();
	void BuildRoundWorld();
	void RunActiveRound();
	void RunRoundIntro();
	void RunRoundEnding();
	void DrawRoundPresentation();
	void DrawWorldWithCamera(Renderer& renderer, const CameraComponent& camera);
	void DrawBattleHUD(Renderer& renderer);
	void RunResultMenu();

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
