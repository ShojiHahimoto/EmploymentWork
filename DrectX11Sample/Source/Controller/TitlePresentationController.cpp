#include "Controller/TitlePresentationController.h"
#include "Component/ModelComponent.h"
#include "Component/MotionPlayerComponent.h"
#include "Component/SkeletonPoseComponent.h"
#include "Core/Debugger.h"
#include "Data/CharacterDataLoader.h"
#include "Data/JsonValue.h"
#include "Resource/ModelResource.h"
#include "System/MotionSystem.h"
#include "System/SpawnDestroySystem.h"
#include "System/TransformSystem.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>

using namespace DirectX::SimpleMath;

/// <summary>待機秒数と攻撃データIDの順序を読み込む。不正設定時は待機のみ再生する。</summary>
void TitlePresentationController::LoadSettings()
{
	waitFrames = 300;
	attackDataIds.clear();
	std::ifstream stream("assets/TitlePresentation.json", std::ios::binary);
	std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	if (text.starts_with("\xEF\xBB\xBF"))
	{
		text.erase(0, 3);
	}

	JsonValue settings;
	std::string error;
	if (!JsonParser::Parse(text, settings, error) || !settings.IsObject())
	{
		DebugLog("[Title] Invalid presentation settings: ", error);
		return;
	}

	const auto *interval = settings.Find("waitSeconds");
	if (interval && interval->IsNumber() && std::isfinite(interval->AsNumber()))
	{
		waitFrames = static_cast<int>(std::round(std::clamp(interval->AsNumber(), 0.0, 3600.0) * 60));
	}

	const auto *attacks = settings.Find("attackDataIds");
	if (attacks && attacks->IsArray())
	{
		for (const auto &id : attacks->AsArray())
		{
			if (id.IsString() && !id.AsString().empty())
			{
				attackDataIds.push_back(id.AsString());
			}
		}
	}
}

/// <summary>展示モデルと設定を初期化し、待機モーションの再生を開始する。</summary>
/// <param name="world">展示用World。</param>
void TitlePresentationController::Enter(World &world)
{
	angle = 0.0f;
	elapsedWaitFrames = 0;
	nextAttackIndex = 0;
	attackPlaying = false;
	LoadSettings();

	ModelResourceManager::LoadModel("DebugPlayer", "assets/model/DebugPlayer/man.fbx", Renderer::GetDevice());
	world.RequestSpawn(SpawnType::DebugPlayer, "TitleCharacter", Vector3(0, 0, 8), Vector3::Zero);
	SpawnDestroySystem::Update(world);
	StartMotion(world, "Common/Idle");
	MotionSystem::Update(world, false);
	TransformSystem::UpdateWorldTransforms(world.GetGameObjects());
}

/// <summary>指定モーションを0Fから開始する。保存されたループ設定を使用する。</summary>
/// <param name="world">展示用World。</param>
/// <param name="motionId">開始するモーションID。</param>
/// <returns>モーションを読み込み、再生対象へ設定できた場合true。</returns>
bool TitlePresentationController::StartMotion(World &world, const std::string &motionId)
{
	if (!MotionDataManager::LoadMotionData(motionId))
	{
		DebugLog("[Title] Motion load failed: ", motionId);
		return false;
	}

	for (auto &object : world.GetGameObjects())
	{
		auto *player = world.GetComponent<MotionPlayerComponent>(object.id);
		if (!player)
		{
			continue;
		}

		player->motionDataId = motionId;
		player->currentFrame = 0;
		player->playing = true;
		player->looping = false;
		player->stateDriven = false;
		player->blending = false;
		player->blendFromBonePoses.clear();
		player->visualRootOffset = Vector3::Zero;
		attackFramesRemaining = std::max(1, MotionDataManager::GetMotionData(motionId)->totalFrames);
		return true;
	}

	return false;
}

/// <summary>待機間隔と攻撃の順次再生を進める。攻撃の移動キーと物理は適用しない。</summary>
/// <param name="world">展示用World。</param>
void TitlePresentationController::Update(World &world)
{
	angle = std::fmod(angle + OrbitRadiansPerFrame, DirectX::XM_2PI);
	if (attackPlaying)
	{
		if (attackFramesRemaining <= 0)
		{
			attackPlaying = false;
			elapsedWaitFrames = 0;
			StartMotion(world, "Common/Idle");
		}
	}
	else if (!attackDataIds.empty() && ++elapsedWaitFrames >= waitFrames)
	{
		elapsedWaitFrames = 0;
		// 壊れた設定で停止しないよう、一巡以内で読み込める技を探す。
		for (size_t attempt = 0; attempt < attackDataIds.size(); ++attempt)
		{
			const std::string id = attackDataIds[nextAttackIndex];
			nextAttackIndex = (nextAttackIndex + 1) % attackDataIds.size();
			if (CharacterDataLoader::LoadAttackData(id, currentAttack) &&
				StartMotion(world, currentAttack.motionDataId))
			{
				attackPlaying = true;
				break;
			}

			DebugLog("[Title] Attack unavailable: ", id);
		}
	}

	MotionSystem::Update(world, false, attackPlaying ? &currentAttack : nullptr);
	if (attackPlaying)
	{
		--attackFramesRemaining;
	}

	TransformSystem::UpdateWorldTransforms(world.GetGameObjects());
}

/// <summary>固定した本体位置に見た目オフセットを加えて描画する。</summary>
/// <param name="world">展示用World。</param>
/// <param name="renderer">描画先。</param>
/// <param name="width">画面幅。</param>
/// <param name="height">画面高さ。</param>
void TitlePresentationController::Draw(World &world, Renderer &renderer, int width, int height) const
{

	using namespace DirectX;
	using namespace DirectX::SimpleMath;
	if (width <= 0 || height <= 0)
	{
		return;
	}
	const Vector3 target(0, TargetHeight, 8);
	const Vector3 eye =
		target + Vector3(std::sin(angle) * OrbitRadius, CameraHeightAboveTarget, -std::cos(angle) * OrbitRadius);
	Matrix view, projection;
	XMStoreFloat4x4(&view, XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMVectorSet(0, 1, 0, 0)));
	XMStoreFloat4x4(&projection, XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), static_cast<float>(width) / height,
														  0.1f, 200.0f));
	renderer.SetViewProjection(view, projection);
	// 周回球全体が部屋の内側に収まる。床・天井・前後左右は既存の共通描画を使う。
	renderer.DrawTrainingRoomBackground(-30, 30, -25, 41, 32, 4);
	for (auto &object : world.GetGameObjects())
	{
		const auto *transform = world.GetTransform(object.id);
		const auto *component = world.GetComponent<ModelComponent>(object.id);
		if (!transform || !component)
		{
			continue;
		}
		const auto *model = ModelResourceManager::GetModel(component->resourceKey);
		const auto *pose = world.GetComponent<SkeletonPoseComponent>(object.id);
		if (model)
		{
			Matrix modelWorld = TransformSystem::GetWorldMatrix(*transform);
			const auto *player = world.GetComponent<MotionPlayerComponent>(object.id);
			if (player)
			{
				modelWorld *= Matrix::CreateTranslation(player->visualRootOffset);
			}

			renderer.DrawModel(*model, modelWorld, pose && pose->initialized ? &pose->skinningMatrices : nullptr);
		}
	}
}
