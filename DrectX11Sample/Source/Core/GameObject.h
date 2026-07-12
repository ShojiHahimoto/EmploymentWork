#pragma once

#include "Component/Component.h"
#include "Core/GameObjectId.h"

#include <memory>
#include <vector>

struct GameObject
{
	// GameObject は ID と Component 群だけを持つデータコンテナ。
	// Component の追加、取得、更新判断は World / System 側で行う。
	GameObjectId id = INVALID_GAME_OBJECT_ID;
	std::vector<std::unique_ptr<Component>> components;
};
