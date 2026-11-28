#include "Core/Game.h"
#include "Manager/SceneManager.h"
#include "Scene/TitleScene.h"
#include "Scene/BattleScene.h"
#include "Input/InputSystem.h"
#include "Core/Application.h"
#include "System/DebugImGuiSystem.h"
#include "Manager/SoundManager.h"
#include "Core/Debugger.h"

#include <memory>
#include "Input/UIInputSystem.h"
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
	if (!GameUI::UIRenderer::Initialize(Renderer::GetDevice(), static_cast<int>(Application::GetWidth()),
									   static_cast<int>(Application::GetHeight())))
	{
		DebugLog("[UI] Dedicated UI renderer initialization failed.");
	}
	SceneManager& sceneManager = SceneManager::GetInstance();
	loadWorker.Start();
	saveWorker.Start();
	SoundManager::GetInstance().Initialize(loadWorker, saveWorker);
	sceneManager.Initialize(loadWorker, saveWorker, static_cast<int>(Application::GetWidth()),
							static_cast<int>(Application::GetHeight()));
	// エントリーシーン設定箇所
	sceneManager.RequestChangeScene(
		std::make_unique<TitleScene>(
			static_cast<int>(Application::GetWidth()),
			static_cast<int>(Application::GetHeight())));
}

void Game::Update()
{
	Input::InputSystem::Update();
	GameUI::UIInputSystem::Update(Application::GetWindow());

	SceneManager& sceneManager = SceneManager::GetInstance();
	sceneManager.Update();
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
	if (!SceneManager::GetInstance().IsLoading())
	{
		DebugImGuiSystem::DrawSoundDebugWindow();
	}
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
	loadWorker.Stop();
	saveWorker.Stop();
	SceneManager::GetInstance().Shutdown();
	Input::InputSystem::Shutdown();
	SoundManager::GetInstance().Shutdown();
	DebugImGuiSystem::Shutdown();
	GameUI::UIInputSystem::Reset();
	GameUI::UIRenderer::Shutdown();
	rendererInitialized = false;

	renderer.Uninit();
}
