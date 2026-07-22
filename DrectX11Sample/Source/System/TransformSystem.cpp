#include "System/TransformSystem.h"
#include <algorithm>

using namespace DirectX;
using namespace DirectX::SimpleMath;

const Vector3& TransformSystem::GetLocalPosition(const TransformComponent& transform)
{
	return transform.localPosition;
}

Vector3 TransformSystem::GetLocalEulerRotationDegrees(const TransformComponent& transform)
{
	// 内部は Quaternion で保持し、外部表示・編集用に Euler 角へ戻す。
	Vector3 eulerRadians = transform.localRotation.ToEuler();

	return Vector3(
		XMConvertToDegrees(eulerRadians.x),
		XMConvertToDegrees(eulerRadians.y),
		XMConvertToDegrees(eulerRadians.z)
	);
}

const Vector3& TransformSystem::GetLocalScale(const TransformComponent& transform)
{
	return transform.localScale;
}

const Vector3& TransformSystem::GetWorldPosition(const TransformComponent& transform)
{
	return transform.worldPosition;
}

const Quaternion& TransformSystem::GetWorldRotation(const TransformComponent& transform)
{
	return transform.worldRotation;
}

const Vector3& TransformSystem::GetWorldScale(const TransformComponent& transform)
{
	return transform.worldScale;
}

const Matrix& TransformSystem::GetWorldMatrix(const TransformComponent& transform)
{
	return transform.worldMatrix;
}

void TransformSystem::SetLocalPosition(TransformComponent& transform, const Vector3& position)
{
	transform.localPosition = position;
	transform.dirty = true;
}

void TransformSystem::SetLocalEulerRotationDegrees(TransformComponent& transform, const Vector3& eulerDegrees)
{
	// 外部からは degree の pitch(X), yaw(Y), roll(Z) として受け取る。
	// SimpleMath の CreateFromYawPitchRoll は yaw, pitch, roll の順で渡す。
	const float pitch = XMConvertToRadians(eulerDegrees.x);
	const float yaw = XMConvertToRadians(eulerDegrees.y);
	const float roll = XMConvertToRadians(eulerDegrees.z);

	transform.localRotation = Quaternion::CreateFromYawPitchRoll(yaw, pitch, roll);
	transform.localRotation.Normalize();
	transform.dirty = true;
}

void TransformSystem::SetLocalRotationQuaternion(TransformComponent& transform, const Quaternion& rotation)
{
	transform.localRotation = rotation;
	transform.localRotation.Normalize();
	transform.dirty = true;
}

void TransformSystem::SetLocalScale(TransformComponent& transform, const Vector3& scale)
{
	transform.localScale = scale;
	transform.dirty = true;
}

bool TransformSystem::SetParent(TransformTable& transforms, GameObjectId childId, GameObjectId parentId, ParentKeepMode keepMode)
{
	// 無効 ID、自分自身の親化、存在しない Transform はすべて拒否する。
	if (childId == INVALID_GAME_OBJECT_ID || parentId == INVALID_GAME_OBJECT_ID || childId == parentId)
	{
		return false;
	}

	TransformComponent* child = FindTransform(transforms, childId);
	TransformComponent* parent = FindTransform(transforms, parentId);

	if (!child || !parent)
	{
		return false;
	}

	if (HasAncestor(transforms, parentId, childId))
	{
		// parentId の祖先に childId がいる場合、設定すると循環階層になる。
		return false;
	}

	// KeepWorldTransform では現在の World 行列が必要になるため、
	// 親子関係を変更する前にキャッシュを最新化しておく。
	UpdateWorldTransformsBeforeHierarchyChange(transforms);

	Matrix childWorld = child->worldMatrix;

	// すでに親がいる場合は、古い親の childIds から先に外す。
	if (child->parentId != INVALID_GAME_OBJECT_ID)
	{
		TransformComponent* oldParent = FindTransform(transforms, child->parentId);
		if (oldParent)
		{
			RemoveChildId(*oldParent, childId);
		}
	}

	child->parentId = parentId;
	AddChildId(*parent, childId);

	if (keepMode == ParentKeepMode::KeepWorldTransform)
	{
		// childWorld = childLocal * parentWorld なので、
		// childLocal = childWorld * inverseParentWorld で逆算する。
		Matrix inverseParentWorld = parent->worldMatrix.Invert();
		SetLocalFromWorldMatrix(*child, childWorld * inverseParentWorld);
	}

	// 親子関係が変わった子以下は World キャッシュを作り直す必要がある。
	MarkDirtyRecursive(transforms, childId);

	return true;
}

bool TransformSystem::RemoveParent(TransformTable& transforms, GameObjectId childId, ParentKeepMode keepMode)
{
	if (childId == INVALID_GAME_OBJECT_ID)
	{
		return false;
	}

	TransformComponent* child = FindTransform(transforms, childId);
	if (!child)
	{
		return false;
	}

	GameObjectId parentId = child->parentId;
	if (parentId == INVALID_GAME_OBJECT_ID)
	{
		return true;
	}

	// 親を外す前の World 行列を保存するため、先に最新化する。
	UpdateWorldTransformsBeforeHierarchyChange(transforms);

	Matrix childWorld = child->worldMatrix;

	TransformComponent* parent = FindTransform(transforms, parentId);
	if (parent)
	{
		RemoveChildId(*parent, childId);
	}

	child->parentId = INVALID_GAME_OBJECT_ID;

	if (keepMode == ParentKeepMode::KeepWorldTransform)
	{
		// 親がなくなるので、今までの World Transform をそのまま Local に移す。
		SetLocalFromWorldMatrix(*child, childWorld);
	}

	MarkDirtyRecursive(transforms, childId);

	return true;
}

void TransformSystem::UpdateWorldTransforms(TransformTable& transforms)
{
	// root から再帰的に更新することで、親の World を先に確定させる。
	for (GameObject& object : transforms)
	{
		TransformComponent* transform = FindTransform(transforms, object.id);
		if (transform && transform->parentId == INVALID_GAME_OBJECT_ID)
		{
			UpdateWorldTransformRecursive(transforms, object.id);
		}
	}
}

void TransformSystem::UpdateWorldTransform(TransformComponent& transform)
{
	ApplyWorldMatrixCache(transform, CreateLocalMatrix(transform));
	transform.dirty = false;
}

void TransformSystem::MarkDirtyRecursive(TransformTable& transforms, GameObjectId objectId)
{
	TransformComponent* transform = FindTransform(transforms, objectId);
	if (!transform)
	{
		return;
	}

	// 親が変わると子孫の World Transform もすべて変わる可能性がある。
	transform->dirty = true;

	for (GameObjectId childId : transform->childIds)
	{
		MarkDirtyRecursive(transforms, childId);
	}
}

void TransformSystem::UpdateWorldTransformsBeforeHierarchyChange(TransformTable& transforms)
{
	UpdateWorldTransforms(transforms);
}

bool TransformSystem::HasAncestor(const TransformTable& transforms, GameObjectId objectId, GameObjectId ancestorId)
{
	// objectId から親方向へ辿り、ancestorId が見つかれば循環候補。
	GameObjectId currentId = objectId;

	while (currentId != INVALID_GAME_OBJECT_ID)
	{
		if (currentId == ancestorId)
		{
			return true;
		}

		const TransformComponent* transform = FindTransform(transforms, currentId);
		if (!transform)
		{
			return false;
		}

		currentId = transform->parentId;
	}

	return false;
}

TransformComponent* TransformSystem::FindTransform(TransformTable& transforms, GameObjectId objectId)
{
	for (GameObject& object : transforms)
	{
		if (object.id != objectId)
		{
			continue;
		}

		for (std::unique_ptr<Component>& component : object.components)
		{
			if (TransformComponent* transform = dynamic_cast<TransformComponent*>(component.get()))
			{
				return transform;
			}
		}
	}

	return nullptr;
}

const TransformComponent* TransformSystem::FindTransform(const TransformTable& transforms, GameObjectId objectId)
{
	for (const GameObject& object : transforms)
	{
		if (object.id != objectId)
		{
			continue;
		}

		for (const std::unique_ptr<Component>& component : object.components)
		{
			if (const TransformComponent* transform = dynamic_cast<const TransformComponent*>(component.get()))
			{
				return transform;
			}
		}
	}

	return nullptr;
}

void TransformSystem::RemoveChildId(TransformComponent& parent, GameObjectId childId)
{
	auto& childIds = parent.childIds;
	childIds.erase(std::remove(childIds.begin(), childIds.end(), childId), childIds.end());
}

void TransformSystem::AddChildId(TransformComponent& parent, GameObjectId childId)
{
	auto it = std::find(parent.childIds.begin(), parent.childIds.end(), childId);
	if (it == parent.childIds.end())
	{
		parent.childIds.push_back(childId);
	}
}

void TransformSystem::UpdateWorldTransformRecursive(TransformTable& transforms, GameObjectId objectId)
{
	TransformComponent* transform = FindTransform(transforms, objectId);
	if (!transform)
	{
		return;
	}

	Matrix localMatrix = CreateLocalMatrix(*transform);

	if (transform->parentId != INVALID_GAME_OBJECT_ID)
	{
		TransformComponent* parent = FindTransform(transforms, transform->parentId);
		if (parent)
		{
			// SimpleMath は S * R * T の行列を local * parentWorld で合成する。
			ApplyWorldMatrixCache(*transform, localMatrix * parent->worldMatrix);
		}
		else
		{
			// 親 Transform が消えていた場合は、孤立 root として扱う。
			transform->parentId = INVALID_GAME_OBJECT_ID;
			ApplyWorldMatrixCache(*transform, localMatrix);
		}
	}
	else
	{
		ApplyWorldMatrixCache(*transform, localMatrix);
	}

	transform->dirty = false;

	for (GameObjectId childId : transform->childIds)
	{
		UpdateWorldTransformRecursive(transforms, childId);
	}
}

Matrix TransformSystem::CreateLocalMatrix(const TransformComponent& transform)
{
	// Transform の基本順序は Scale -> Rotation -> Translation。
	return Matrix::CreateScale(transform.localScale)
		* Matrix::CreateFromQuaternion(transform.localRotation)
		* Matrix::CreateTranslation(transform.localPosition);
}

void TransformSystem::ApplyWorldMatrixCache(TransformComponent& transform, const Matrix& worldMatrix)
{
	// 行列だけでなく、よく使う World 位置・回転・スケールもキャッシュする。
	transform.worldMatrix = worldMatrix;
	transform.worldMatrix.Decompose(transform.worldScale, transform.worldRotation, transform.worldPosition);
	transform.worldRotation.Normalize();
}

void TransformSystem::SetLocalFromWorldMatrix(TransformComponent& transform, const Matrix& worldMatrix)
{
	// Decompose に失敗した場合でも古い値を残さないよう、先に既定値へ戻す。
	transform.localPosition = Vector3::Zero;
	transform.localRotation = Quaternion::Identity;
	transform.localScale = Vector3::One;

	Matrix decomposedMatrix = worldMatrix;
	decomposedMatrix.Decompose(transform.localScale, transform.localRotation, transform.localPosition);
	transform.localRotation.Normalize();
	transform.dirty = true;
}
