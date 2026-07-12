#pragma once

#include "Component/TransformComponent.h"
#include "Core/GameObject.h"

enum class ParentKeepMode
{
	// 親を付け替えても localPosition / localRotation / localScale を維持する。
	// 見た目の World Transform は親の影響で変わる。
	KeepLocalTransform,

	// 親を付け替えても見た目の World Transform を維持する。
	// そのために local Transform を逆算して書き換える。
	KeepWorldTransform,
};

class TransformSystem
{
public:
	// World が持つ GameObject 群を受け取り、必要な TransformComponent を参照する。
	using TransformTable = std::vector<GameObject>;

	// Local 値は親から見た相対値。
	static const DirectX::SimpleMath::Vector3& GetLocalPosition(const TransformComponent& transform);
	static DirectX::SimpleMath::Vector3 GetLocalEulerRotationDegrees(const TransformComponent& transform);
	static const DirectX::SimpleMath::Vector3& GetLocalScale(const TransformComponent& transform);

	// World 値は UpdateWorldTransforms 後に有効になるキャッシュ。
	static const DirectX::SimpleMath::Vector3& GetWorldPosition(const TransformComponent& transform);
	static const DirectX::SimpleMath::Quaternion& GetWorldRotation(const TransformComponent& transform);
	static const DirectX::SimpleMath::Vector3& GetWorldScale(const TransformComponent& transform);
	static const DirectX::SimpleMath::Matrix& GetWorldMatrix(const TransformComponent& transform);

	// 回転は内部 Quaternion、外部 API では Euler 角 degree も扱えるようにする。
	static void SetLocalPosition(TransformComponent& transform, const DirectX::SimpleMath::Vector3& position);
	static void SetLocalEulerRotationDegrees(TransformComponent& transform, const DirectX::SimpleMath::Vector3& eulerDegrees);
	static void SetLocalRotationQuaternion(TransformComponent& transform, const DirectX::SimpleMath::Quaternion& rotation);
	static void SetLocalScale(TransformComponent& transform, const DirectX::SimpleMath::Vector3& scale);

	// 親子関係の変更は循環参照を防ぐため、必ず System 経由で行う。
	static bool SetParent(TransformTable& transforms, GameObjectId childId, GameObjectId parentId, ParentKeepMode keepMode);
	static bool RemoveParent(TransformTable& transforms, GameObjectId childId, ParentKeepMode keepMode);

	// 親を持たない root から順に、全 Transform の World キャッシュを更新する。
	static void UpdateWorldTransforms(TransformTable& transforms);

private:
	static void MarkDirtyRecursive(TransformTable& transforms, GameObjectId objectId);
	static void UpdateWorldTransformsBeforeHierarchyChange(TransformTable& transforms);
	static bool HasAncestor(const TransformTable& transforms, GameObjectId objectId, GameObjectId ancestorId);
	static TransformComponent* FindTransform(TransformTable& transforms, GameObjectId objectId);
	static const TransformComponent* FindTransform(const TransformTable& transforms, GameObjectId objectId);
	static void RemoveChildId(TransformComponent& parent, GameObjectId childId);
	static void AddChildId(TransformComponent& parent, GameObjectId childId);
	static void UpdateWorldTransformRecursive(TransformTable& transforms, GameObjectId objectId);
	static DirectX::SimpleMath::Matrix CreateLocalMatrix(const TransformComponent& transform);
	static void ApplyWorldMatrixCache(TransformComponent& transform, const DirectX::SimpleMath::Matrix& worldMatrix);
	static void SetLocalFromWorldMatrix(TransformComponent& transform, const DirectX::SimpleMath::Matrix& worldMatrix);
};
