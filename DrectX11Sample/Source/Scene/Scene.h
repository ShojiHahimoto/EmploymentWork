#pragma once

#include "Resource/ModelResource.h"
#include "Data/CharacterData.h"
#include "Data/MotionData.h"
#include "Data/EffectData.h"
#include <wrl/client.h>

class Renderer;
class World;
class SceneManager;
class LoadWorker;
class SaveWorker;
struct SceneLoadRequest;

class Scene
{
public:
	virtual ~Scene() = default;

	virtual void Enter() = 0;
	virtual void DescribeLoad(SceneLoadRequest &request) const;
	virtual void Exit() = 0;

	// Scene 自体にゲームロジックを持たせず、System 群を固定順で実行する入口。
	virtual void Update() = 0;
	virtual void Draw(Renderer& renderer) = 0;
	virtual void OnResize(int width, int height) = 0;

	virtual World& GetWorld() = 0;
	virtual const World& GetWorld() const = 0;
	ID3D11ShaderResourceView *FindSceneTexture(const std::string &path) const;

protected:
	LoadWorker *loadWorker = nullptr;
	SaveWorker *saveWorker = nullptr;
	std::shared_ptr<const CharacterData> FindSceneCharacter(const std::string &folder) const;
	const ModelResource *FindSceneModel(const std::string &key) const;

private:
	friend class SceneManager;
	friend class ModelResourceManager;
	friend class MotionDataManager;
	friend class EffectDataManager;
	std::unordered_map<std::string, EffectData> sceneEffects;
	std::unordered_map<std::string, std::unique_ptr<MotionData>> sceneMotions;
	std::unordered_map<std::string, std::shared_ptr<const CharacterData>> sceneCharacters;
	std::unordered_map<std::string, std::unique_ptr<ModelResource>> sceneModels;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> sceneTextures;
};
