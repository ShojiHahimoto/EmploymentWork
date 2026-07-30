#include "System/TransformSystem.h"
#include <algorithm>

using namespace DirectX;
using namespace DirectX::SimpleMath;

/// <summary>
/// TransformComponent のローカル座標を取得する。
/// </summary>
/// <param name="transform">参照する TransformComponent。</param>
/// <returns>ローカル座標。</returns>
const Vector3& TransformSystem::GetLocalPosition(const TransformComponent& transform)
{
	return transform.localPosition;
}

/// <summary>
/// Quaternion で保持しているローカル回転を、編集用の Euler 角度に変換して取得する。
/// </summary>
/// <param name="transform">参照する TransformComponent。</param>
/// <returns>degree 単位のローカル Euler 回転。</returns>
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

/// <summary>
/// TransformComponent のローカルスケールを取得する。
/// </summary>
/// <param name="transform">参照する TransformComponent。</param>
/// <returns>ローカルスケール。</returns>
const Vector3& TransformSystem::GetLocalScale(const TransformComponent& transform)
{
	return transform.localScale;
}

/// <summary>
/// 更新済みキャッシュから World 座標を取得する。
/// </summary>
/// <param name="transform">参照する TransformComponent。</param>
/// <returns>World 座標。</returns>
const Vector3& TransformSystem::GetWorldPosition(const TransformComponent& transform)
{
	return transform.worldPosition;
}

/// <summary>
/// 更新済みキャッシュから World 回転を取得する。
/// </summary>
/// <param name="transform">参照する TransformComponent。</param>
/// <returns>World 回転の Quaternion。</returns>
const Quaternion& TransformSystem::GetWorldRotation(const TransformComponent& transform)
{
	return transform.worldRotation;
}

/// <summary>
/// 更新済みキャッシュから World スケールを取得する。
/// </summary>
/// <param name="transform">参照する TransformComponent。</param>
/// <returns>World スケール。</returns>
const Vector3& TransformSystem::GetWorldScale(const TransformComponent& transform)
{
	return transform.worldScale;
}

/// <summary>
/// 更新済みキャッシュから World 行列を取得する。
/// </summary>
/// <param name="transform">参照する TransformComponent。</param>
/// <returns>World 行列。</returns>
const Matrix& TransformSystem::GetWorldMatrix(const TransformComponent& transform)
{
	return transform.worldMatrix;
}

/// <summary>
/// TransformComponent のローカル座標を設定し、World キャッシュ更新が必要な状態にする。
/// </summary>
/// <param name="transform">変更する TransformComponent。</param>
/// <param name="position">設定するローカル座標。</param>
void TransformSystem::SetLocalPosition(TransformComponent& transform, const Vector3& position)
{
	transform.localPosition = position;
	transform.dirty = true;
}

/// <summary>
/// degree 単位の Euler 角を Quaternion に変換してローカル回転へ設定する。
/// </summary>
/// <param name="transform">変更する TransformComponent。</param>
/// <param name="eulerDegrees">pitch(X)、yaw(Y)、roll(Z) の degree 角度。</param>
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

/// <summary>
/// Quaternion を直接ローカル回転へ設定する。
/// </summary>
/// <param name="transform">変更する TransformComponent。</param>
/// <param name="rotation">設定するローカル回転 Quaternion。</param>
void TransformSystem::SetLocalRotationQuaternion(TransformComponent& transform, const Quaternion& rotation)
{
	transform.localRotation = rotation;
	transform.localRotation.Normalize();
	transform.dirty = true;
}

/// <summary>
/// TransformComponent のローカルスケールを設定し、World キャッシュ更新が必要な状態にする。
/// </summary>
/// <param name="transform">変更する TransformComponent。</param>
/// <param name="scale">設定するローカルスケール。</param>
void TransformSystem::SetLocalScale(TransformComponent& transform, const Vector3& scale)
{
	transform.localScale = scale;
	transform.dirty = true;
}

/// <summary>
/// 指定 Transform を別 Transform の子に設定する。
/// </summary>
/// <param name="transforms">親子関係を検索・更新する GameObject 配列。</param>
/// <param name="childId">子にする GameObject の ID。</param>
/// <param name="parentId">親にする GameObject の ID。</param>
/// <param name="keepMode">親子付け時に World Transform を維持するかどうか。</param>
/// <returns>親子付けに成功した場合は true。</returns>
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

/// <summary>
/// 指定 Transform の親を外す。
/// </summary>
/// <param name="transforms">親子関係を検索・更新する GameObject 配列。</param>
/// <param name="childId">親を外す GameObject の ID。</param>
/// <param name="keepMode">親を外す時に World Transform を維持するかどうか。</param>
/// <returns>親外しに成功した場合は true。</returns>
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

/// <summary>
/// Transform 階層全体の World キャッシュを root から順に更新する。
/// </summary>
/// <param name="transforms">更新対象の GameObject 配列。</param>
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

/// <summary>
/// 親を持たない単体 Transform の World キャッシュを更新する。
/// </summary>
/// <param name="transform">更新する TransformComponent。</param>
void TransformSystem::UpdateWorldTransform(TransformComponent& transform)
{
	ApplyWorldMatrixCache(transform, CreateLocalMatrix(transform));
	transform.dirty = false;
}

/// <summary>
/// 指定 Transform と子孫 Transform を再帰的に dirty 状態にする。
/// </summary>
/// <param name="transforms">親子関係を検索する GameObject 配列。</param>
/// <param name="objectId">dirty にする起点 GameObject の ID。</param>
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

/// <summary>
/// 親子関係変更前に、KeepWorldTransform 用の World キャッシュを最新化する。
/// </summary>
/// <param name="transforms">更新対象の GameObject 配列。</param>
void TransformSystem::UpdateWorldTransformsBeforeHierarchyChange(TransformTable& transforms)
{
	UpdateWorldTransforms(transforms);
}

/// <summary>
/// 指定 Transform の親方向を辿り、特定 ID が祖先にいるか確認する。
/// </summary>
/// <param name="transforms">親子関係を検索する GameObject 配列。</param>
/// <param name="objectId">祖先検索の起点 GameObject ID。</param>
/// <param name="ancestorId">祖先として含まれるか確認する GameObject ID。</param>
/// <returns>ancestorId が祖先に含まれていれば true。</returns>
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

/// <summary>
/// 指定 GameObject から TransformComponent を検索する。
/// </summary>
/// <param name="transforms">検索対象の GameObject 配列。</param>
/// <param name="objectId">検索する GameObject の ID。</param>
/// <returns>見つかった TransformComponent。存在しない場合は nullptr。</returns>
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

/// <summary>
/// 指定 GameObject から TransformComponent を読み取り専用で検索する。
/// </summary>
/// <param name="transforms">検索対象の GameObject 配列。</param>
/// <param name="objectId">検索する GameObject の ID。</param>
/// <returns>見つかった TransformComponent。存在しない場合は nullptr。</returns>
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

/// <summary>
/// 親 Transform の childIds から指定子 ID を取り除く。
/// </summary>
/// <param name="parent">子 ID を削除する親 TransformComponent。</param>
/// <param name="childId">削除する子 GameObject の ID。</param>
void TransformSystem::RemoveChildId(TransformComponent& parent, GameObjectId childId)
{
	auto& childIds = parent.childIds;
	childIds.erase(std::remove(childIds.begin(), childIds.end(), childId), childIds.end());
}

/// <summary>
/// 親 Transform の childIds に指定子 ID を重複なしで追加する。
/// </summary>
/// <param name="parent">子 ID を追加する親 TransformComponent。</param>
/// <param name="childId">追加する子 GameObject の ID。</param>
void TransformSystem::AddChildId(TransformComponent& parent, GameObjectId childId)
{
	auto it = std::find(parent.childIds.begin(), parent.childIds.end(), childId);
	if (it == parent.childIds.end())
	{
		parent.childIds.push_back(childId);
	}
}

/// <summary>
/// 指定 Transform と子孫 Transform の World キャッシュを再帰的に更新する。
/// </summary>
/// <param name="transforms">更新対象の GameObject 配列。</param>
/// <param name="objectId">更新起点の GameObject ID。</param>
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

/// <summary>
/// TransformComponent のローカル座標・回転・スケールからローカル行列を作る。
/// </summary>
/// <param name="transform">行列化する TransformComponent。</param>
/// <returns>Scale、Rotation、Translation を合成したローカル行列。</returns>
Matrix TransformSystem::CreateLocalMatrix(const TransformComponent& transform)
{
	// Transform の基本順序は Scale -> Rotation -> Translation。
	return Matrix::CreateScale(transform.localScale)
		* Matrix::CreateFromQuaternion(transform.localRotation)
		* Matrix::CreateTranslation(transform.localPosition);
}

/// <summary>
/// World 行列を TransformComponent に保存し、World 座標・回転・スケールのキャッシュも更新する。
/// </summary>
/// <param name="transform">キャッシュを書き込む TransformComponent。</param>
/// <param name="worldMatrix">保存する World 行列。</param>
void TransformSystem::ApplyWorldMatrixCache(TransformComponent& transform, const Matrix& worldMatrix)
{
	// 行列だけでなく、よく使う World 位置・回転・スケールもキャッシュする。
	transform.worldMatrix = worldMatrix;
	transform.worldMatrix.Decompose(transform.worldScale, transform.worldRotation, transform.worldPosition);
	transform.worldRotation.Normalize();
}

/// <summary>
/// World 行列を分解し、親基準へ変換済みのローカル Transform として保存する。
/// </summary>
/// <param name="transform">ローカル値を書き込む TransformComponent。</param>
/// <param name="worldMatrix">分解する World 行列。</param>
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
