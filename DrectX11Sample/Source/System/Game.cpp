#include "System/Game.h"
#include "Scene/SceneManager.h"
#include "Scene/TitleScene.h"
#include "Scene/BattleScene.h"
#include "Input/InputSystem.h"
#include "System/Application.h"
#include "System/DebugImGuiSystem.h"
#include "System/SoundManager.h"
#include "System/Debugger.h"

#include <memory>
#include "UI/UIInputSystem.h"
#include "UI/UIRenderer.h"

void Game::Init()
{
	rendererInitialized = SUCCEEDED(renderer.Init());
	if (!rendererInitialized)
	{
		return;
	}

	DebugImGuiSystem::Init(
		Application::GetWindow(),
		Renderer::GetDevice(),
		Renderer::GetDeviceContext());

	Input::InputSystem::Initialize();
	GameUI::UIInputSystem::Reset();
	if (GameUI::UIRenderer::Initialize(Renderer::GetDevice(), static_cast<int>(Application::GetWidth()), static_cast<int>(Application::GetHeight())))
	{
		GameUI::UIRenderer::LoadAssets("assets/texture/UI/Parts/assets.json");
	}
	else
	{
		DebugLog("[UI] Dedicated UI renderer initialization failed.");
	}
	SoundManager::GetInstance().Initialize();

	SceneManager& sceneManager = SceneManager::GetInstance();
	// エントリーシーン設定箇所
	sceneManager.RequestChangeScene(
		std::make_unique<TitleScene>(
			static_cast<int>(Application::GetWidth()),
			static_cast<int>(Application::GetHeight())));
	sceneManager.ApplyPendingSceneChange();
}

void Game::Update()
{
	Input::InputSystem::Update();
	GameUI::UIInputSystem::Update(Application::GetWindow());

	SceneManager& sceneManager = SceneManager::GetInstance();
	sceneManager.RunSystems();
	sceneManager.ApplyPendingSceneChange();
	SoundManager::GetInstance().Update();
}

void Game::Draw()
{
	if (!rendererInitialized)
	{
		return;
	}

	renderer.DrawStart();
	DebugImGuiSystem::BeginFrame();
	SceneManager::GetInstance().Draw(renderer);
	DebugImGuiSystem::DrawSoundDebugWindow();
	DebugImGuiSystem::Render();
	renderer.DrawEnd();
}

void Game::OnResize(int width, int height)
{
	if (!rendererInitialized || width <= 0 || height <= 0)
	{
		return;
	}

	if (FAILED(Renderer::ResizeWindow(width, height)))
	{
		rendererInitialized = false;
		return;
	}

	SceneManager::GetInstance().OnResize(width, height);
	GameUI::UIRenderer::Resize(width, height);
}

void Game::Uninit()
{
	SceneManager::GetInstance().Shutdown();
	Input::InputSystem::Shutdown();
	SoundManager::GetInstance().Shutdown();
	DebugImGuiSystem::Shutdown();
	GameUI::UIInputSystem::Reset();
	GameUI::UIRenderer::Shutdown();
	rendererInitialized = false;

	renderer.Uninit();
}
