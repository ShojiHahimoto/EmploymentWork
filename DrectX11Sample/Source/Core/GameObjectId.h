#pragma once

#include <cstdint>

// GameObject を直接ポインタで参照しないための識別子。
// World / System 間の関係表現は、この ID を経由して行う。
using GameObjectId = uint32_t;

// 0 は「存在しない GameObject」を表す予約値。
// 実際に生成する GameObjectId は 1 以上にする。
constexpr GameObjectId INVALID_GAME_OBJECT_ID = 0;
