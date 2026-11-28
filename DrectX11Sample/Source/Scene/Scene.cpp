#include "Scene/Scene.h"

/// <summary>シーン固有の読み込み要求を追加する。既定の実装では何も追加しない。</summary>
/// <param name="request">遷移前に準備する素材とデータの要求。</param>
void Scene::DescribeLoad([[maybe_unused]] SceneLoadRequest &request) const
{
}

/// <summary>シーンに登録済みのキャラクターデータを取得する。</summary>
/// <param name="folder">ロード要求に指定したキャラクターフォルダ。</param>
/// <returns>準備済みデータ。未登録ならnullptr。</returns>
std::shared_ptr<const CharacterData> Scene::FindSceneCharacter(const std::string &folder) const
{
	const auto character = sceneCharacters.find(folder);
	return character == sceneCharacters.end() ? nullptr : character->second;
}

/// <summary>シーンが所有するモデルを検索する。</summary>
/// <param name="key">ロード要求に指定したモデルキー。</param>
/// <returns>シーンが所有するモデルへの借用ポインタ。未登録ならnullptr。</returns>
const ModelResource *Scene::FindSceneModel(const std::string &key) const
{
	const auto model = sceneModels.find(key);
	return model == sceneModels.end() ? nullptr : model->second.get();
}

/// <summary>シーンが所有するGPU画像を検索する。</summary>
/// <param name="path">ロード要求に指定した画像パス。</param>
/// <returns>シーンが所有するビューへの借用ポインタ。未登録ならnullptr。</returns>
ID3D11ShaderResourceView *Scene::FindSceneTexture(const std::string &path) const
{
	const auto texture = sceneTextures.find(path);
	return texture == sceneTextures.end() ? nullptr : texture->second.Get();
}
