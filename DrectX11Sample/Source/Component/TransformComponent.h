#pragma once

#include "Core/GameObjectId.h"
#include <SimpleMath.h>
#include <vector>

struct TransformComponent
{
	// 親から見た相対 Transform。
	// parentId が INVALID の場合は、Local と World が同じ意味になる。
	DirectX::SimpleMath::Vector3 localPosition = DirectX::SimpleMath::Vector3::Zero;
	DirectX::SimpleMath::Quaternion localRotation = DirectX::SimpleMath::Quaternion::Identity;
	DirectX::SimpleMath::Vector3 localScale = DirectX::SimpleMath::Vector3::One;

	// 描画やカメラ計算で使うための World Transform キャッシュ。
	// 値の更新は TransformComponent ではなく TransformSystem が担当する。
	DirectX::SimpleMath::Matrix worldMatrix = DirectX::SimpleMath::Matrix::Identity;
	DirectX::SimpleMath::Vector3 worldPosition = DirectX::SimpleMath::Vector3::Zero;
	DirectX::SimpleMath::Quaternion worldRotation = DirectX::SimpleMath::Quaternion::Identity;
	DirectX::SimpleMath::Vector3 worldScale = DirectX::SimpleMath::Vector3::One;

	// 親子関係はポインタではなく GameObjectId で持つ。
	// 追加・削除・循環チェックは TransformSystem 側で行う。
	GameObjectId parentId = INVALID_GAME_OBJECT_ID;
	std::vector<GameObjectId> childIds;

	// Local 値や親子関係が変わり、World キャッシュの再計算が必要な状態。
	bool dirty = true;
};
