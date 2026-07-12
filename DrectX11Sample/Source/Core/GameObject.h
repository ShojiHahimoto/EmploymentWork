#pragma once

#include "Component/Component.h"

#include <cstdint>
#include <memory>
#include <vector>

// GameObject を直接ポインタで参照しないための識別子。
using GameObjectId = uint32_t;

// 0 は「存在しない GameObject」を表す予約値。
// 実際に生成する GameObjectId は 1 以上にする。
constexpr GameObjectId INVALID_GAME_OBJECT_ID = 0;

struct GameObject
{
	// GameObject は ID と Component 群だけを持つデータコンテナ。
	// Component の追加、取得、更新判断は World / System 側で行う。
	GameObjectId id = INVALID_GAME_OBJECT_ID;
	std::vector<std::unique_ptr<Component>> components;
};
