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

/// <summary>待機秒数と攻撃データを、GPUや現在のシーンに触れずに読み込む。</summary>
/// <returns>タイトル演出の設定。不正な設定や技は読み飛ばす。</returns>
PreparedTitlePresentation TitlePresentationController::Prepare()
{
	PreparedTitlePresentation result;
	std::ifstream stream("assets/TitlePresentation.json", std::ios::binary);
	if (!stream)
	{
		stream.open("DrectX11Sample/assets/TitlePresentation.json", std::ios::binary);
	}

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

		return result;
	}

	const auto *interval = settings.Find("waitSeconds");
	if (interval && interval->IsNumber() && std::isfinite(interval->AsNumber()))
	{
		result.waitFrames = static_cast<int>(std::round(std::clamp(interval->AsNumber(), 0.0, 3600.0) * 60));
	}

	const auto *attacks = settings.Find("attackDataIds");
	if (attacks && attacks->IsArray())
	{
		for (const auto &id : attacks->AsArray())
		{
			if (id.IsString() && !id.AsString().empty())
			{
				AttackData attack;
				if (CharacterDataLoader::LoadAttackData(id.AsString(), attack))
				{
					result.attacks.push_back(std::move(attack));
				}
			}
		}
	}

	return result;
}

/// <summary>展示モデルと設定を初期化し、待機モーションの再生を開始する。</summary>
/// <param name="world">展示用World。</param>
/// <param name="prepared">ワーカーが準備した演出設定。</param>
/// <param name="character">展示キャラクターの準備済みデータ。</param>
void TitlePresentationController::Enter(World &world, const PreparedTitlePresentation &prepared,
										std::shared_ptr<const CharacterData> character)
{
	angle = 0.0f;
	elapsedWaitFrames = 0;
	nextAttackIndex = 0;
	attackPlaying = false;
	waitFrames = prepared.waitFrames;
	attacks = prepared.attacks;

	world.RequestSpawn(SpawnType::DebugPlayer, "TitleCharacter", Vector3(0, 0, 8), Vector3::Zero, "",
					   std::move(character));
	SpawnDestroySystem::Update(world);
	StartMotion(world, "Common/Idle");
	MotionSystem::Update(world, false);
	TransformSystem::UpdateWorldTransforms(world.GetGameObjects());
}

/// <summary>指定モーションを0Fから開始する。保存されたループ設定を使用する。</summary>
/// <param name="world">展示用World。</param>
/// <param name="motionId">開始するモーションID。
/// 再生するモーションID。</param>
/// <returns>モーションを読み込み、再生対象へ設定できた場合true。
/// モーションを取得できた場合true。</returns>
/// <remarks>モーションを0Fから開始する。攻撃は単発、待機は保存されたループ設定に従う。</remarks>
bool TitlePresentationController::StartMotion(World &world, const std::string &motionId)
{
	const auto *motion = MotionDataManager::GetMotionData(motionId);
	if (!motion)
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
		attackFramesRemaining = std::max(1, motion->totalFrames);
		return true;
	}

	return false;
}

/// <summary>待機間隔と攻撃の順次再生を進める。攻撃の移動キーと物理は適用しない。</summary>
/// <param name="world">展示用World。</param>
/// <remarks>固定60fpsで演出を更新する。対戦入力・物理・攻撃移動は実行しない。</remarks>
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
	else if (!attacks.empty() && ++elapsedWaitFrames >= waitFrames)
	{
		elapsedWaitFrames = 0;
		// 壊れた設定で停止しないよう、一巡以内で再生可能な技を探す。
		for (size_t attempt = 0; attempt < attacks.size(); ++attempt)
		{
			const auto &attack = attacks[nextAttackIndex];
			nextAttackIndex = (nextAttackIndex + 1) % attacks.size();
			if (StartMotion(world, attack.motionDataId))
			{
				currentAttack = attack;
				attackPlaying = true;
				break;
			}

			DebugLog("[Title] Attack unavailable: ", attack.attackDataId);
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
/// <remarks>中央固定の本体行列に、汎用モーションの見た目オフセットだけを加えて描く。</remarks>
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
