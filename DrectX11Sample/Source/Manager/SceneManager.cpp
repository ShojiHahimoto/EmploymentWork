#include "Manager/SceneManager.h"

#include "Core/Debugger.h"
#include "Core/Renderer.h"
#include "Data/CharacterDataLoader.h"
#include "Data/MotionDataLoader.h"
#include "Data/EffectDataLoader.h"

#include <algorithm>
#include <chrono>
#include <exception>

/// <summary>
/// SceneManager の唯一のインスタンスを取得する。
/// </summary>
/// <returns>SceneManager の参照。</returns>
SceneManager& SceneManager::GetInstance()
{
	static SceneManager instance;
	return instance;
}

/// <summary>共通ロードワーカーを借り、遷移専用画像を準備する。</summary>
/// <param name="worker">Game が起動・停止を管理するロードワーカー。</param>
/// <param name="writer">Game が起動・停止を管理する保存専用ワーカー。</param>
/// <param name="width">初期画面幅。</param>
/// <param name="height">初期画面高さ。</param>
void SceneManager::Initialize(LoadWorker &worker, SaveWorker &writer, int width, int height)
{
	loadWorker = &worker;
	saveWorker = &writer;
	this->width = width;
	this->height = height;

	if (FAILED(Renderer::LoadTextureFromFile("assets/texture/Load_kari.png", loadingTexture.ReleaseAndGetAddressOf())) &&
		FAILED(Renderer::LoadTextureFromFile("DrectX11Sample/assets/texture/Load_kari.png",
										   loadingTexture.ReleaseAndGetAddressOf())))
	{
		DebugLog("[Loading] Loading image unavailable.");
	}

}

/// <summary>
/// 次のシーンを予約し、その素材のCPU準備を依頼する。
/// </summary>
/// <param name="nextScene">次に有効化する Scene。</param>
void SceneManager::RequestChangeScene(std::unique_ptr<Scene> nextScene)
{
	if (!nextScene || pendingScene || !loadWorker)
	{
		return;
	}

	SceneLoadRequest request;
	nextScene->DescribeLoad(request);
	if (!GameUI::UIRenderer::IsInitialized())
	{
		request.uiImageIds.clear();
	}
	std::erase_if(request.models, [](const SceneModelLoad &model)
	{
		return model.sharedPlayer && ModelResourceManager::GetModel(model.key);
	});

	pendingScene = std::move(nextScene);
	pendingBgmId = request.bgmId;
	loadingDrawn = false;

	SoundManager::GetInstance().StopBGM();

	if (!request.bgmId.empty() || !request.models.empty() || !request.textures.empty() || !request.optionalTextures.empty() || !request.characterFolders.empty() ||
		!request.motionIds.empty() || request.prepareScene || !request.uiImageIds.empty() || !request.effectIds.empty())
	{
		using PreparationTask = std::packaged_task<std::optional<PreparedAssets>()>;
		auto prepare = std::make_shared<PreparationTask>([request = std::move(request)]
		{
			return PrepareAssets(request);
		});
		preparedAssets = prepare->get_future();
		loadWorker->Submit(std::packaged_task<void()>([prepare]
		{
			(*prepare)();
		}), LoadPriority::SceneTransition);
	}
}

/// <summary>モデルと画像をGPUやシーンに触れずに読み込む。</summary>
/// <param name="request">次のシーンに必要な素材一覧。</param>
/// <returns>準備済みの素材。必須素材の読み込み失敗時は空。</returns>
std::optional<SceneManager::PreparedAssets> SceneManager::PrepareAssets(const SceneLoadRequest &request)
{
	PreparedAssets result;
	if (!request.bgmId.empty())
	{
		result.bgm = SoundManager::PrepareSound(request.bgmId);
	}

	if (!request.uiImageIds.empty())
	{
		result.uiImages = GameUI::UIRenderer::PrepareAssets("assets/texture/UI/Parts/assets.json", request.uiImageIds);
		if (result.uiImages.empty())
		{
			return std::nullopt;
		}
	}

	//モデルを読み込む
	for (const auto &model : request.models)
	{
		PreparedModelData data;
		if (!ModelResource::PrepareFromFile(model.path, data) &&
			!ModelResource::PrepareFromFile("DrectX11Sample/" + model.path, data))
		{
			DebugLog("[Loading] Model preparation failed: ", model.path);

			return std::nullopt;
		}

		result.models.emplace_back(model, std::move(data));
	}

	//画像を読み込む
	auto texturePaths = request.textures;
	for (const auto& path : request.optionalTextures)
	{
		if (std::find(texturePaths.begin(), texturePaths.end(), path) == texturePaths.end())
		{
			texturePaths.push_back(path);
		}
	}
	for (const auto &id : request.effectIds)
	{
		EffectData effect;
		if (!EffectDataLoader::LoadEffectData(id, effect))
		{
			continue;
		}

		for (const auto &emitter : effect.emitters)
		{
			if (!emitter.texturePath.empty() &&
				std::find(texturePaths.begin(), texturePaths.end(), emitter.texturePath) == texturePaths.end())
			{
				texturePaths.push_back(emitter.texturePath);
			}
		}

		result.effects.emplace(id, std::move(effect));
	}

	for (const auto &path : texturePaths)
	{
		TexturePreparation::Image image;
		const std::wstring widePath(path.begin(), path.end());
		if (FAILED(TexturePreparation::DecodeFile(widePath.c_str(), image)) &&
			FAILED(TexturePreparation::DecodeFile((L"DrectX11Sample/" + widePath).c_str(), image)))
		{
			DebugLog("[Loading] Texture preparation failed: ", path);

			if (std::find(request.textures.begin(), request.textures.end(), path) != request.textures.end())
			{
				return std::nullopt;
			}

			continue;
		}

		result.textures.emplace_back(path, std::move(image));
	}

	//キャラクターを読み込む
	for (const auto &folder : request.characterFolders)
	{
		if (result.characters.contains(folder))
		{
			continue;
		}

		auto character = std::make_shared<CharacterData>();
		if (!CharacterDataLoader::LoadCharacterData(folder, *character))
		{
			DebugLog("[Loading] Character preparation incomplete; using defaults for missing data: ", folder);
		}

		result.characters.emplace(folder, std::move(character));
	}

	const auto prepareMotion = [&result](const std::string &id)
	{
		if (id.empty() || result.motions.contains(id))
		{
			return;
		}

		auto motion = std::make_unique<MotionData>();
		if (!MotionDataLoader::LoadMotionData(id, *motion))
		{
			// 欠落を記録して、技の初回再生で同じファイルを再探索しない。
			motion.reset();
		}

		result.motions.emplace(id, std::move(motion));
	};

	//モーションを読み込む
	for (const auto &id : request.motionIds)
	{
		prepareMotion(id);
	}

	for (const auto &[folder, character] : result.characters)
	{
		for (const auto &assigned : character->attacks)
		{
			prepareMotion(assigned.attack.motionDataId);
		}
	}

	if (request.prepareScene)
	{
		auto prepared = request.prepareScene();
		for (const auto &id : prepared.motionIds)
		{
			prepareMotion(id);
		}

		result.applySceneData = std::move(prepared.apply);
	}

	//読み込み結果を返す
	return result;
}

/// <summary>準備済み素材をGPU登録し、次のシーンまたは共通モデルキャッシュへ渡す。</summary>
/// <param name="assets">ワーカーから受け取ったCPUデータ。</param>
/// <returns>必須素材のGPU登録に成功した場合はtrue。</returns>
bool SceneManager::PublishAssets(PreparedAssets &&assets)
{
	auto *device = Renderer::GetDevice();
	std::vector<std::pair<std::string, std::unique_ptr<ModelResource>>> sharedPlayers;

	for (auto &[request, data] : assets.models)
	{
		auto model = std::make_unique<ModelResource>();
		if (!model->Upload(device, std::move(data)))
		{
			return false;
		}

		if (request.sharedPlayer)
		{
			sharedPlayers.emplace_back(request.key, std::move(model));
		}
		else
		{
			pendingScene->sceneModels.emplace(request.key, std::move(model));
		}
	}

	for (const auto &[path, image] : assets.textures)
	{
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
		if (FAILED(TexturePreparation::Upload(device, image, view.GetAddressOf())))
		{
			return false;
		}

		pendingScene->sceneTextures.emplace(path, std::move(view));
	}

	for (auto &[key, model] : sharedPlayers)
	{
		ModelResourceManager::RegisterPreparedPlayer(key, std::move(model));
	}

	pendingScene->sceneCharacters = std::move(assets.characters);
	pendingScene->sceneMotions = std::move(assets.motions);
	pendingScene->sceneEffects = std::move(assets.effects);
	if (assets.applySceneData)
	{
		assets.applySceneData(*pendingScene);
	}

	if (!GameUI::UIRenderer::UploadAssets(assets.uiImages))
	{
		return false;
	}

	if (!pendingBgmId.empty() &&
		!SoundManager::GetInstance().RegisterSound(pendingBgmId, std::move(assets.bgm), true, true))
	{
		DebugLog("[Sound] Scene BGM unavailable: ", pendingBgmId);
	}

	return true;
}

/// <summary>
/// 予約されている Scene 切り替えを適用し、旧 Scene の終了処理と新 Scene の開始処理を行う。
/// </summary>
void SceneManager::ApplyPendingSceneChange()
{
	if (!pendingScene || !loadingDrawn)
	{
		return;
	}

	const bool loadHadAssets = preparedAssets.valid();
	if (loadHadAssets)
	{
		if (preparedAssets.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
		{
			return;
		}

		bool published = false;
		try
		{
			auto assets = preparedAssets.get();
			published = assets && PublishAssets(std::move(*assets));
		}
		catch (const std::exception &error)
		{
			DebugLog("[Loading] ", error.what());
		}

		if (!published)
		{
			DebugLog("[Loading] Scene transition canceled.");
			pendingScene.reset();
			pendingBgmId.clear();
			SoundManager::GetInstance().PlayBGM(currentBgmId);

			return;
		}
	}

	if (!loadHadAssets)
	{
		GameUI::UIRenderer::UnloadAssets();
	}

	if (currentScene)
	{
		currentScene->Exit();
		currentScene.reset();
	}

	currentScene = std::move(pendingScene);
	currentBgmId = std::move(pendingBgmId);
	currentScene->loadWorker = loadWorker;
	currentScene->saveWorker = saveWorker;

	currentScene->OnResize(width, height);
	currentScene->Enter();
	SoundManager::GetInstance().PlayBGM(currentBgmId);
}

/// <summary>
/// 現在有効な Scene を取得する。
/// </summary>
/// <returns>現在の Scene。存在しない場合は nullptr。</returns>
Scene* SceneManager::GetCurrentScene()
{
	return currentScene.get();
}

/// <summary>
/// 現在有効な Scene を読み取り専用で取得する。
/// </summary>
/// <returns>現在の Scene。存在しない場合は nullptr。</returns>
const Scene* SceneManager::GetCurrentScene() const
{
	return currentScene.get();
}

/// <summary>
/// 現在有効な Scene が存在するか確認する。
/// </summary>
/// <returns>Scene が存在すれば true。</returns>
bool SceneManager::HasScene() const
{
	return currentScene != nullptr;
}

void SceneManager::Update()
{
	if (currentScene && !IsLoading())
	{
		currentScene->Update();
	}
}

/// <summary>
/// 現在の Scene の描画処理を呼び出す。
/// </summary>
/// <param name="renderer">Scene 描画に使う Renderer。</param>
void SceneManager::Draw(Renderer& renderer)
{
	if (IsLoading())
	{
		Renderer::DrawFullscreenTexture(loadingTexture.Get(), width, height);
		loadingDrawn = true;

		return;
	}

	if (currentScene)
	{
		currentScene->Draw(renderer);
	}
}

/// <summary>
/// ウィンドウサイズ変更を現在の Scene に通知する。
/// </summary>
/// <param name="width">新しい幅。</param>
/// <param name="height">新しい高さ。</param>
void SceneManager::OnResize(int width, int height)
{
	this->width = width;
	this->height = height;

	if (currentScene)
	{
		currentScene->OnResize(width, height);
	}

	if (pendingScene)
	{
		pendingScene->OnResize(width, height);
	}
}

/// <summary>
/// 予約 Scene と現在 Scene を破棄し、現在 Scene があれば終了処理を呼ぶ。
/// </summary>
void SceneManager::Shutdown()
{
	loadWorker = nullptr;
	saveWorker = nullptr;
	preparedAssets = {};
	pendingScene.reset();
	loadingTexture.Reset();

	if (currentScene)
	{
		currentScene->Exit();
		currentScene.reset();
	}
}

/// <summary>遷移先の準備または切り替えを待っているか確認する。</summary>
/// <returns>遷移が予約中ならtrue。</returns>
bool SceneManager::IsLoading() const
{
	return pendingScene != nullptr;
}
