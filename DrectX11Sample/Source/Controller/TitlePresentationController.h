#pragma once

#include "World/World.h"
#include "Component/ModelComponent.h"
#include "Component/SkeletonPoseComponent.h"
#include "Resource/ModelResource.h"
#include "System/MotionSystem.h"
#include "System/SpawnDestroySystem.h"
#include "System/TransformSystem.h"
#include "System/Renderer.h"
#include <DirectXMath.h>
#include <cmath>

/// <summary>タイトル専用の部屋・モデル・固定フレーム周回演出。対戦処理は実行しない。</summary>
class TitlePresentationController
{
public:
	/// <summary>既存プレイヤー定義と共有モデルを使い、中央に展示用オブジェクトを生成する。</summary>
	/// <param name="world">Sceneが所有する展示用World。</param>
	void Enter(World& world)
	{
		angle = 0.0f;
		ModelResourceManager::LoadModel("DebugPlayer", "assets/model/DebugPlayer/man.fbx", Renderer::GetDevice());
		world.RequestSpawn(SpawnType::DebugPlayer, "TitleCharacter",
			DirectX::SimpleMath::Vector3(0, 0, 8), DirectX::SimpleMath::Vector3::Zero);
		SpawnDestroySystem::Update(world);
		Update(world);
	}

	/// <summary>モーションだけを進める。入力・物理・攻撃は動かさず、中心座標を維持する。</summary>
	/// <param name="world">展示オブジェクトを保持するWorld。</param>
	void Update(World& world)
	{
		angle = std::fmod(angle + OrbitRadiansPerFrame, DirectX::XM_2PI);
		MotionSystem::Update(world);
		TransformSystem::UpdateWorldTransforms(world.GetGameObjects());
	}

	/// <summary>全面のグリッド部屋と展示モデルを周回カメラで描く。</summary>
	/// <param name="world">展示用World。</param>
	/// <param name="renderer">既存3Dレンダラー。</param>
	/// <param name="width">画面幅。</param>
	/// <param name="height">画面高さ。</param>
	void Draw(World& world, Renderer& renderer, int width, int height) const
	{
		using namespace DirectX;
		using namespace DirectX::SimpleMath;
		if (width <= 0 || height <= 0) return;
		const Vector3 target(0, TargetHeight, 8);
		const Vector3 eye = target + Vector3(std::sin(angle) * OrbitRadius, CameraHeightAboveTarget, -std::cos(angle) * OrbitRadius);
		Matrix view, projection;
		XMStoreFloat4x4(&view, XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMVectorSet(0, 1, 0, 0)));
		XMStoreFloat4x4(&projection, XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), static_cast<float>(width) / height, 0.1f, 200.0f));
		renderer.SetViewProjection(view, projection);
		// 周回球全体が部屋の内側に収まる。床・天井・前後左右は既存の共通描画を使う。
		renderer.DrawTrainingRoomBackground(-30, 30, -25, 41, 32, 4);
		for (auto& object : world.GetGameObjects())
		{
			const auto* transform = world.GetTransform(object.id);
			const auto* component = world.GetComponent<ModelComponent>(object.id);
			if (!transform || !component) continue;
			const auto* model = ModelResourceManager::GetModel(component->resourceKey);
			const auto* pose = world.GetComponent<SkeletonPoseComponent>(object.id);
			if (model) renderer.DrawModel(*model, TransformSystem::GetWorldMatrix(*transform),
				pose && pose->initialized ? &pose->skinningMatrices : nullptr);
		}
	}

private:
	// 調整する場合はここへ集約。60fps固定で一周約42秒。見た目のルート移動は適用しない。
	static constexpr float OrbitRadiansPerFrame = 0.0025f;
	static constexpr float OrbitRadius = 23.0f;
	static constexpr float CameraHeightAboveTarget = 7.0f;
	static constexpr float TargetHeight = 5.0f;
	float angle = 0; // 固定フレームで進む周回角度。
};
