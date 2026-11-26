#pragma once

#include <string>
#include <vector>
#include <functional>

class Scene;

struct PreparedSceneLoad
{
	std::vector<std::string> motionIds;
	std::function<void(Scene &)> apply;
};

struct SceneModelLoad
{
	std::string key;
	std::string path;
	bool sharedPlayer = false;
};

struct SceneLoadRequest
{
	// CPU準備はシーンに触れず、apply は遷移先が確定するまで実行しない。
	std::function<PreparedSceneLoad()> prepareScene;
	std::vector<std::string> uiImageIds;
	std::vector<std::string> effectIds;
	std::vector<SceneModelLoad> models;			//モデル
	std::vector<std::string> textures;			//テクスチャ
	std::vector<std::string> optionalTextures;	//欠落しても遷移できる差し替え素材
	std::vector<std::string> characterFolders;	//使用キャラクター、攻撃データを含む
	std::vector<std::string> motionIds;			//モーションデータ
};
