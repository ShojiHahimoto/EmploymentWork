#pragma once

#include "Scene/Scene.h"
#include "Manager/LoadWorker.h"
#include "Manager/SaveWorker.h"
#include "Manager/SceneLoadRequest.h"
#include "Manager/SoundManager.h"
#include "UI/UIRenderer.h"
#include <memory>
#include <optional>

class SceneManager
{
public:
	static SceneManager& GetInstance();
	void Initialize(LoadWorker &worker, SaveWorker &writer, int width, int height);

	void RequestChangeScene(std::unique_ptr<Scene> nextScene);
	void ApplyPendingSceneChange();

	Scene* GetCurrentScene();
	const Scene* GetCurrentScene() const;
	bool HasScene() const;

	void Update();
	void Draw(Renderer& renderer);
	void OnResize(int width, int height);
	void Shutdown();
	bool IsLoading() const;

private:
	SceneManager() = default;
	~SceneManager() = default;

	SceneManager(const SceneManager&) = delete;
	SceneManager& operator=(const SceneManager&) = delete;

	std::unique_ptr<Scene> currentScene;
	std::unique_ptr<Scene> pendingScene;
	std::string currentBgmId;
	std::string pendingBgmId;

	/// <summary>
	/// 準備した素材をここに格納する
	/// </summary>
	struct PreparedAssets
	{
		GameUI::UIRenderer::PreparedImages uiImages;
		SoundManager::PreparedSound bgm;
		std::unordered_map<std::string, EffectData> effects;
		std::vector<std::pair<SceneModelLoad, PreparedModelData>> models;
		std::vector<std::pair<std::string, TexturePreparation::Image>> textures;
		std::unordered_map<std::string, std::shared_ptr<const CharacterData>> characters;
		std::unordered_map<std::string, std::unique_ptr<MotionData>> motions;
		std::function<void(Scene &)> applySceneData;
	};

	LoadWorker *loadWorker = nullptr;
	SaveWorker *saveWorker = nullptr;
	std::future<std::optional<PreparedAssets>> preparedAssets;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> loadingTexture;
	int width = 0;
	int height = 0;
	bool loadingDrawn = false;

	static std::optional<PreparedAssets> PrepareAssets(const SceneLoadRequest &request);
	bool PublishAssets(PreparedAssets &&assets);
};
