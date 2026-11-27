#include "Scene/BattleScene.h"

#include "Component/BattleCameraFollowComponent.h"
#include "Component/BattleTimerComponent.h"
#include "Component/CharacterAttackDataComponent.h"
#include "Component/HealthGaugeComponent.h"
#include "Component/HitBoxComponent.h"
#include "Component/ModelComponent.h"
#include "Component/MotionPlayerComponent.h"
#include "Component/SkeletonPoseComponent.h"
#include "Component/StateComponent.h"
#include "Component/VelocityComponent.h"
#include "Data/EffectDataLoader.h"
#include "Input/InputSystem.h"
#include "Resource/ModelResource.h"
#include "Scene/BattleSetupScene.h"
#include "Scene/TitleScene.h"
#include "Manager/SceneManager.h"
#include "System/BattleCameraSystem.h"
#include "System/BattleHUDSystem.h"
#include "System/BattleResultSystem.h"
#include "System/CameraSystem.h"
#include "System/CommandInputSystem.h"
#include "System/DebugCameraControlSystem.h"
#include "System/DebugImGuiSystem.h"
#include "Core/Debugger.h"
#include "System/EmbedResolveSystem.h"
#include "System/EffectRenderSystem.h"
#include "System/EffectSystem.h"
#include "System/HitCollisionSystem.h"
#include "System/HitReactionSystem.h"
#include "System/HitResolveSystem.h"
#include "System/HitStopSystem.h"
#include "System/InputHistorySystem.h"
#include "System/MovementSystem.h"
#include "System/MotionSystem.h"
#include "System/PlayerInvincibilitySystem.h"
#include "System/PlayerFacingSystem.h"
#include "System/PlayerControlSystem.h"
#include "Core/Renderer.h"
#include "Manager/SoundManager.h"
#include "System/SpawnDestroySystem.h"
#include "System/StateUpdateSystem.h"
#include "System/TransformSystem.h"

#include <algorithm>
#include <memory>
#include <vector>

using namespace DirectX::SimpleMath;

namespace
{
	constexpr const char* GoldArmorTexture = "assets/model/Player_01/AlloyAtlas.gold.color.png";
	constexpr float ResultBackdropOpacity = 0.48f;
	constexpr const char* RoundBanners[] = {
		"assets/texture/UI/Parts/Battle/Round_1.png",
		"assets/texture/UI/Parts/Battle/Round_2.png",
		"assets/texture/UI/Parts/Battle/Final_Round.png",
		"assets/texture/UI/Parts/Battle/Fight.png",
		"assets/texture/UI/Parts/Battle/KO.png",
		"assets/texture/UI/Parts/Battle/Double_KO.png",
		"assets/texture/UI/Parts/Battle/Time_Over.png",
		"assets/texture/UI/Parts/Battle/Player1_Win.png",
		"assets/texture/UI/Parts/Battle/Player2_Win.png",
		"assets/texture/UI/Parts/Battle/Draw.png"
	};

	/// <summary>向きに応じた見た目オフセットを含め、通常描画と深度描画で共有するモデルの位置と姿勢を取り出す。</summary>
	/// <param name="world">描画対象のWorld。</param>
	/// <param name="id">モデルを持つオブジェクトID。</param>
	/// <param name="transform">対象のTransform。</param>
	/// <returns>描画するモデル、ワールド行列、ボーン行列。モデルがない場合は空。</returns>
	Renderer::ShadowCaster GetModelDrawData(World &world, GameObjectId id, const TransformComponent &transform)
	{
		const auto *component = world.GetComponent<ModelComponent>(id);
		if (!component)
		{
			return {};
		}

		Renderer::ShadowCaster data;
		data.model = ModelResourceManager::GetModel(component->resourceKey);
		data.world = TransformSystem::GetWorldMatrix(transform);
		const auto *pose = world.GetComponent<SkeletonPoseComponent>(id);
		data.skinningMatrices = pose && pose->initialized ? &pose->skinningMatrices : nullptr;

		const auto *motion = world.GetComponent<MotionPlayerComponent>(id);
		if (motion && motion->visualRootOffset != Vector3::Zero)
		{
			Vector3 drawOffset = motion->visualRootOffset;
			const auto *state = world.GetComponent<StateComponent>(id);
			if (state && state->facingDirection == FacingDirection::Left)
			{
				drawOffset.x = -drawOffset.x;
			}

			data.world = data.world * Matrix::CreateTranslation(drawOffset);
		}

		return data;
	}
}

/// <summary>
/// BattleScene を現在の描画サイズで初期化する。
/// </summary>
/// <param name="initialWidth">初期ウィンドウ幅。</param>
/// <param name="initialHeight">初期ウィンドウ高さ。</param>
BattleScene::BattleScene(int initialWidth, int initialHeight)
	: setupData(BattleSetup::CreateDefaultBattleSetupData())
	, width(initialWidth)
	, height(initialHeight)
{
}

/// <summary>
/// BattleScene を現在の描画サイズとバトル開始設定で初期化する。
/// </summary>
/// <param name="initialWidth">初期ウィンドウ幅。</param>
/// <param name="initialHeight">初期ウィンドウ高さ。</param>
/// <param name="battleSetupData">1P/2Pの入力デバイスとキャラ選択情報。</param>
BattleScene::BattleScene(int initialWidth, int initialHeight, const BattleSetup::BattleSetupData& battleSetupData)
	: setupData(battleSetupData)
	, width(initialWidth)
	, height(initialHeight)
{
}

/// <summary>対戦開始前に必要なモデル、画像、音源、対戦データを要求する。</summary>
/// <param name="request">ワーカーへ渡すロード要求。</param>
void BattleScene::DescribeLoad(SceneLoadRequest &request) const
{
	request.models.push_back({"Debugman", "assets/model/Debugman/Akai.fbx"});
	request.textures.emplace_back("assets/texture/number.png");
	request.uiImageIds = {"button", "button_primary"};
	for (const char* path : RoundBanners)
	{
		request.textures.emplace_back(path);
	}
	for (const char* path : BattleHUDSystem::TexturePaths)
	{
		request.textures.emplace_back(path);
	}
	request.optionalTextures.emplace_back(GoldArmorTexture);
	request.effectIds = {"HitSpark", "GuardSpark"};
	request.prepareScene = []
	{
		auto battleMusic = std::make_shared<SoundManager::PreparedSound>(
			SoundManager::PrepareSound(SoundIds::BattleBgm01));
		auto normalHit = std::make_shared<SoundManager::PreparedSound>(
			SoundManager::PrepareSound(SoundIds::HitNormal));
		auto hardHit = std::make_shared<SoundManager::PreparedSound>(
			SoundManager::PrepareSound(SoundIds::HitHard));

		PreparedSceneLoad result;
		result.apply = [battleMusic, normalHit, hardHit](Scene &)
		{
			auto &sound = SoundManager::GetInstance();
			sound.RegisterSound(SoundIds::BattleBgm01, std::move(*battleMusic), true, true);
			sound.RegisterSound(SoundIds::HitNormal, std::move(*normalHit), false, false);
			sound.RegisterSound(SoundIds::HitHard, std::move(*hardHit), false, false);
		};

		return result;
	};
	request.motionIds = {"Common/Idle", "Common/WalkForward", "Common/WalkBack", "Common/Crouch",
						 "Common/Guard", "Common/CrouchGuard", "Common/JumpStart", "Common/JumpLoop",
						 "Common/Hitstun", "Common/AirHitstun", "Common/AirToDown", "Common/Down", "Common/Wakeup", "Common/Win"};

	for (const auto &player : setupData.players)
	{
		request.characterFolders.push_back(player.characterFolderPath.empty()
			? BattleSetup::BuildCharacterFolderPath(0) : player.characterFolderPath);
	}
}

/// <summary>準備済み素材を使って対戦用のWorldとカメラを初期化する。</summary>
void BattleScene::Enter()
{
	//キーコンを取得
	Input::InputSystem::SetBindings(BattleSetup::BuildInputBindings(setupData));
	//アクションマップをゲームプレイに変更
	Input::InputSystem::SetActionMap(Input::InputActionMapId::Gameplay);

	//ラウンド情報を初期化
	round = {};
	resultMenu = {};
	visibleRoundPoints = {};
	//ラウンド中の演出の時間を取得
	roundTiming = BattleResultSystem::LoadTiming();
	//サウンド初期化
	//ラウンドごとにサウンドが再起動しないように他とは別で初期化する
	InitializeBattleSounds();
#if defined(_DEBUG)
	InitializeDebugSceneView();
#endif
	//ラウンドを開始する
	BuildRoundWorld();
}

/// <summary>試合の得点と対戦設定を保持し、ラウンド用のプレイヤー・カメラ・HUDを初期化する。</summary>
void BattleScene::BuildRoundWorld()
{
	//ワールドを初期化
	world.Clear();
	//カメラを生成
	GameObjectId cameraId = world.CreateTransform("MainCamera");
	world.AddComponent<BattleCameraFollowComponent>(cameraId);
	TransformComponent* cameraTransform = world.GetTransform(cameraId);
	if (cameraTransform)
	{
		TransformSystem::SetLocalPosition(*cameraTransform, Vector3(0.0f, 8.0f, -20.0f));
		TransformSystem::SetLocalEulerRotationDegrees(*cameraTransform, Vector3(0.0f, 0.0f, 0.0f));
	}

	//追跡対象にプレイヤーを登録
	const auto preparedCharacter = [this](const std::string &folder)
	{
		return FindSceneCharacter(folder.empty() ? BattleSetup::BuildCharacterFolderPath(0) : folder);
	};

	//プレイヤー生成
	world.RequestSpawn(
		SpawnType::DebugPlayer,
		"DebugPlayer",
		Vector3(-4.0f, 0.0f, 8.0f),
		Vector3(0.0f, 0.0f, 0.0f),
		setupData.players[0].characterFolderPath,
		preparedCharacter(setupData.players[0].characterFolderPath));

	world.RequestSpawn(
		SpawnType::DebugPlayer2,
		"DebugPlayer2",
		Vector3(4.0f, 0.0f, 8.0f),
		Vector3(0.0f, 0.0f, 0.0f),
		setupData.players[1].characterFolderPath,
		preparedCharacter(setupData.players[1].characterFolderPath));

	//world.RequestSpawn(
	//	SpawnType::DebugCube,
	//	"DebugCube",
	//	Vector3(0.0f, 0.0f, 6.0f),
	//	Vector3(20.0f, 32.0f, 0.0f));

	//HUD初期化
	InitializeBattleHUD();

	//カメラシステムにカメラ登録
	CameraComponent camera;
	const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
	CameraSystem::SetPerspective(camera, 45.0f, aspectRatio, 0.1f, 1000.0f);
	world.SetActiveCamera(cameraId, camera);

	introFrame = 0;
	endingFrame = 0;
	fadeFrame = -1;
	postStopMotionStarted = false;
	victoryMotionStarted = false;
	roundBanner = nullptr;
	//ラウンド演出による遅延を計算
	const int fadeIn = round.number > 1 ? roundTiming.fadeInFrames : 0;
	BattleResultSystem::BeginRound(world, round,
		fadeIn + roundTiming.roundIntroDelayFrames + roundTiming.roundDisplayFrames +
		roundTiming.roundToFightFrames + roundTiming.fightDisplayFrames + roundTiming.fightToControlFrames);
	RunInitialWorldSetup();
}

/// <summary>
/// BattleScene が保持するデバッグ描画リソースと World を破棄する。
/// </summary>
void BattleScene::Exit()
{
#if defined(_DEBUG)
	Renderer::ReleaseRenderTexture(sceneViewRenderTexture);
#endif
	hudNumberTexture = nullptr;
	EffectRenderSystem::ReleaseResources();
	SoundManager::GetInstance().StopBGM();
	world.Clear();
}

void BattleScene::Update()
{
#if defined(_DEBUG)
	UpdateDebugSceneViewCamera();
#endif

	//決着がついた場合ラウンド終了演出を開始
	if (resultMenu.active)
	{
		RunResultMenu();
	}
	else if (round.endReason != RoundEndReason::None)
	{
		RunRoundEnding();
	}
	//待機時間が発生している場合待機時間の処理を進める
	else if (round.waitFrames > 0)
	{
		RunRoundIntro();
	}
	//通常のラウンド処理（バトル処理）
	else
	{
		RunActiveRound();
	}
}

/// <summary>ラウンド開始時の演出。開幕表示と待機モーションを進め、最後の待ち時間まで対戦操作を止める。</summary>
void BattleScene::RunRoundIntro()
{
	//演出時間を計算
	const auto& timing = roundTiming;
	const int fadeIn = round.number > 1 ? timing.fadeInFrames : 0;
	const int roundStart = fadeIn + timing.roundIntroDelayFrames;
	const int fightStart = roundStart + timing.roundDisplayFrames + timing.roundToFightFrames;
	//ラウンド開始時のバナーを選択する
	roundBanner = nullptr;
	if (introFrame >= roundStart && introFrame < roundStart + timing.roundDisplayFrames)
	{
		roundBanner = RoundBanners[std::clamp(round.number - 1, 0, 2)];
	}
	else if (introFrame >= fightStart && introFrame < fightStart + timing.fightDisplayFrames)
	{
		roundBanner = RoundBanners[3];
	}

	//演出による操作停止中もモーションは更新する
	MotionSystem::Update(world);
	++introFrame;
	//待機時間計測を進める
	BattleResultSystem::AdvanceWait(world, round);
}

/// <summary>ラウンド決着演出。操作を停止したまま、決着表示・モーション・暗転を順に進める。</summary>
void BattleScene::RunRoundEnding()
{
	//時間を計測
	const auto& timing = roundTiming;
	const bool doubleKO = round.endReason == RoundEndReason::DoubleKnockOut;
	const bool timeOver = round.endReason == RoundEndReason::TimeOver;
	const int displayEnd = timing.roundEndDisplayDelayFrames + timing.roundEndDisplayFrames;
	const int release = timeOver ? 0 : displayEnd;
	const auto result = world.GetBattleResult();
	const auto match = BattleResultSystem::GetMatchResult(round);
	const int winner = result == BattleResult::Player1Win ? 0 : (result == BattleResult::Player2Win ? 1 : -1);
	const int victoryStart = std::max(displayEnd + timing.roundEndAfterDisplayFrames, release +
		(doubleKO ? timing.doubleKnockoutDownFrames : timing.releaseToVictoryFrames));
	const bool matchFinished = match != BattleResult::None;
	const int victoryFrames = winner >= 0 ? timing.victoryMotionFrames : 0;
	const int framesBeforeFade = matchFinished ? timing.matchResultDisplayFrames : victoryFrames;

	//バナーを選択する
	roundBanner = nullptr;
	if (fadeFrame < 0 && endingFrame >= timing.roundEndDisplayDelayFrames && endingFrame < displayEnd)
	{
		roundBanner = RoundBanners[timeOver ? 6 : (doubleKO ? 5 : 4)];
	}

	if (!postStopMotionStarted && endingFrame >= release)
	{
		world.ClearHitStop();
		for (int side = 0; side < World::BattlePlayerCount; ++side)
		{
			const auto id = world.GetBattlePlayerId(side);
			auto* motion = world.GetComponent<MotionPlayerComponent>(id);
			auto* state = world.GetComponent<StateComponent>(id);
			if (motion && state)
			{
				const bool down = !timeOver && (doubleKO || side != winner);
				const bool airborne = !state->isGrounded;
				state->currentActionState = airborne ? PlayerActionState::Fall
					: (down ? PlayerActionState::Down : PlayerActionState::Idle);
				if (auto* velocity = world.GetComponent<VelocityComponent>(id))
				{
					MovementSystem::SetVelocityX(*velocity, 0);
					MovementSystem::SetVelocityZ(*velocity, 0);
				}

				*motion = {};
				motion->motionDataId = airborne ? "Common/JumpLoop" : (down ? "Common/Down" : "Common/Idle");
				motion->looping = airborne || !down;
				motion->holdFinalFrame = !airborne && down;
			}
		}

		BattleHUDSystem::Update(world, width, height, false);
		postStopMotionStarted = true;
	}

	if (fadeFrame < 0 && endingFrame >= victoryStart)
	{
		visibleRoundPoints = round.points;
		const int victoryFrame = endingFrame - victoryStart;
		if (!victoryMotionStarted && winner >= 0 && (matchFinished || victoryFrames > 0))
		{
			auto* motion = world.GetComponent<MotionPlayerComponent>(world.GetBattlePlayerId(winner));
			if (motion)
			{
				*motion = {};
				motion->motionDataId = "Common/Win";
				motion->looping = false;
				victoryMotionStarted = true;
			}
		}

		if (matchFinished && victoryFrame < framesBeforeFade)
		{
			roundBanner = RoundBanners[match == BattleResult::Draw ? 9 : (match == BattleResult::Player1Win ? 7 : 8)];
		}

		bool skip = false;
		const bool canSkip = matchFinished
			? victoryFrame < framesBeforeFade
			: victoryMotionStarted && victoryFrame > 0 && victoryFrame < victoryFrames;
		if (canSkip)
		{
			for (int side = 0; side < World::BattlePlayerCount; ++side)
			{
				for (int action = 0; action < static_cast<int>(Input::InputActionId::Count); ++action)
				{
					skip |= Input::InputSystem::GetActionState(side, static_cast<Input::InputActionId>(action)).trigger;
				}
			}
		}

		if (skip || victoryFrame >= framesBeforeFade)
		{
			roundBanner = nullptr;
			fadeFrame = 0;
		}
	}

	if (postStopMotionStarted)
	{
		MovementSystem::Update(world);
		EmbedResolveSystem::Update(world);
		for (int side = 0; side < World::BattlePlayerCount; ++side)
		{
			const auto id = world.GetBattlePlayerId(side);
			auto* motion = world.GetComponent<MotionPlayerComponent>(id);
			const auto* state = world.GetComponent<StateComponent>(id);
			if (motion && state && state->isGrounded && motion->motionDataId == "Common/JumpLoop")
			{
				const bool down = !timeOver && (doubleKO || side != winner);
				*motion = {};
				motion->motionDataId = down ? "Common/AirToDown" : "Common/Idle";
				motion->holdFinalFrame = down;
			}
		}

		MotionSystem::Update(world, false);
		TransformSystem::UpdateWorldTransforms(world.GetGameObjects());
	}

	if (fadeFrame >= 0)
	{
		if (fadeFrame >= timing.fadeOutFrames)
		{
			if (match != BattleResult::None)
			{
				resultMenu.active = true;
				Input::InputSystem::SetActionMap(Input::InputActionMapId::UI);
				fadeFrame = -1;
				roundBanner = RoundBanners[match == BattleResult::Draw ? 9 : (match == BattleResult::Player1Win ? 7 : 8)];
			}
			else
			{
				++round.number;
				BuildRoundWorld();
			}

			return;
		}

		++fadeFrame;
	}

	++endingFrame;
}

/// <summary>ラウンド演出の文字と暗転を描画する。</summary>
void BattleScene::DrawRoundPresentation()
{
	if (roundBanner)
	{
		auto* texture = FindSceneTexture(roundBanner);
		int textureWidth = 0;
		int textureHeight = 0;
		Renderer::GetTextureSize(texture, textureWidth, textureHeight);
		const float scale = std::min(width / 1600.0f, height / 900.0f) * 0.53f;
		const int w = static_cast<int>(textureWidth * scale);
		const int h = static_cast<int>(textureHeight * scale);
		const int x = (width - w) / 2;
		const int y = static_cast<int>(height * 0.31f);
		Renderer::DrawTextureRegion(texture, {x, y, x + w, y + h}, {0, 0, textureWidth, textureHeight},
			Color(1, 1, 1, 1));
	}

	float alpha = 0.0f;
	if (fadeFrame >= 0)
	{
		alpha = static_cast<float>(fadeFrame) / std::max(1, roundTiming.fadeOutFrames);
		if (BattleResultSystem::GetMatchResult(round) != BattleResult::None)
		{
			alpha *= ResultBackdropOpacity;
		}
	}
	else if (round.number > 1 && round.waitFrames > 0 && introFrame < roundTiming.fadeInFrames)
	{
		alpha = 1.0f - static_cast<float>(introFrame) / roundTiming.fadeInFrames;
	}

	if (alpha > 0.0f)
	{
		Renderer::DrawScreenRect({0, 0, width, height}, Color(0, 0, 0, alpha));
	}
}

/// <summary>操作可能なラウンドを1フレーム進め、ダメージ反映後に決着を判定する。</summary>
void BattleScene::RunActiveRound()
{
	roundBanner = nullptr;
	SpawnDestroySystem::Update(world);
	const bool hitStopActive = world.IsHitStopActive();

	if (!hitStopActive)
	{
		PlayerFacingSystem::Update(world);
	}

	InputHistorySystem::Update(world);
	CommandInputSystem::Update(world);

	// ヒットストップ中は対戦オブジェクトの状態・移動・判定・モーションだけを止める。
	// 入力履歴、タイマー、HUD、Debug 表示は止めず、硬直明けの先行入力や演出確認を保つ。
	if (!hitStopActive)
	{
		StateUpdateSystem::Update(world);
		PlayerControlSystem::Update(world);
		MovementSystem::Update(world);
		BattleCameraSystem::Update(world);
		EmbedResolveSystem::Update(world);
		PlayerInvincibilitySystem::Update(world);
		HitCollisionSystem::Update(world);
		HitResolveSystem::Update(world);
		HitReactionSystem::Update(world);
		PlayerInvincibilitySystem::Update(world);
		MotionSystem::Update(world);
		TransformSystem::UpdateWorldTransforms(world.GetGameObjects());
	}

	BattleResultSystem::Update(world, round);
	if (round.endReason != RoundEndReason::None && round.endReason != RoundEndReason::TimeOver)
	{
		world.ClearHitStop();
		world.RequestHitStop(HitStopLevel::SpecialAttack,
			roundTiming.roundEndDisplayDelayFrames + roundTiming.roundEndDisplayFrames);
	}
	EffectSystem::Update(world);
	BattleHUDSystem::Update(world, width, height);

	if (hitStopActive && round.endReason == RoundEndReason::None)
	{
		HitStopSystem::Update(world);
	}

	if (world.HasActiveCamera())
	{
		CameraComponent& camera = world.GetActiveCamera();
		TransformComponent* cameraTransform = world.GetTransform(world.GetActiveCameraId());
		if (cameraTransform)
		{
			CameraSystem::Update(camera, *cameraTransform);
		}
	}

}

/// <summary>
/// メインカメラで BattleScene の World を描画し、Debug ビルドでは SceneView と ImGui も描画する。
/// </summary>
/// <param name="renderer">描画に使用する Renderer。</param>
void BattleScene::Draw(Renderer& renderer)
{
	if (!world.HasActiveCamera())
	{
		return;
	}

	const CameraComponent& camera = world.GetActiveCamera();
	std::vector<Renderer::ShadowCaster> casters;
	casters.reserve(2);
	for (const auto &object : world.GetGameObjects())
	{
		if (object.tag != GameObjectTag::Player)
		{
			continue;
		}

		const auto *transform = world.GetTransform(object.id);
		if (transform)
		{
			casters.push_back(GetModelDrawData(world, object.id, *transform));
		}
	}

	const float stageCenterX = (EmbedResolveSystem::StageMinX + EmbedResolveSystem::StageMaxX) * 0.5f;
	renderer.DrawShadowMap(casters, Vector3(stageCenterX, 5.0f, 8.0f),
						   EmbedResolveSystem::StageMaxX - EmbedResolveSystem::StageMinX + 38.0f);
	DrawWorldWithCamera(renderer, camera);
	EffectRenderSystem::Draw(world, camera);

#if defined(_DEBUG)
	DrawDebugSceneView(renderer);
	renderer.SetViewProjection(camera.viewMatrix, camera.projectionMatrix);
	DrawDebugHitBoxes(renderer);
#endif

	DrawBattleHUD(renderer);
	if (resultMenu.active)
	{
		Renderer::DrawScreenRect({0, 0, width, height}, Color(0, 0, 0, ResultBackdropOpacity));
	}

	DrawRoundPresentation();
	if (resultMenu.active)
	{
		BattleHUDSystem::DrawResultMenu(renderer, width, height, setupData, resultMenu);
	}

	DebugImGuiSystem::DrawSpawnWindow(world);
	DebugImGuiSystem::DrawWorldInspector(world);
#if defined(_DEBUG)
	DebugImGuiSystem::DrawHitBoxDebugWindow();
#endif
}

/// <summary>
/// 指定カメラの View / Projection を使って World 内の描画対象を描画する。
/// </summary>
/// <param name="renderer">描画に使用する Renderer。</param>
/// <param name="camera">描画視点として使う CameraComponent。</param>
void BattleScene::DrawWorldWithCamera(Renderer& renderer, const CameraComponent& camera)
{
	renderer.SetViewProjection(camera.viewMatrix, camera.projectionMatrix);
	renderer.DrawTrainingRoomBackground(EmbedResolveSystem::StageMinX, EmbedResolveSystem::StageMaxX,
										-30.0f, 56.0f, 48.0f, 5.0f, true);

	const auto secondPlayer = world.GetBattlePlayerId(1);
	const Renderer::ModelAlbedoOverride goldArmor{"AlloyAtlas.color.png", FindSceneTexture(GoldArmorTexture)};
	for (GameObject& object : world.GetGameObjects())
	{
		if (object.id == world.GetActiveCameraId())
		{
			continue;
		}

		if (object.tag == GameObjectTag::UI || object.tag == GameObjectTag::Effect)
		{
			continue;
		}

		TransformComponent* transform = world.GetTransform(object.id);
		if (transform)
		{
			const auto data = GetModelDrawData(world, object.id, *transform);
			const auto* armor = object.id == secondPlayer ? &goldArmor : nullptr;
			if (data.model && renderer.DrawModel(*data.model, data.world, Color(1, 1, 1, 1), false,
				data.skinningMatrices, armor))
			{
				continue;
			}

			renderer.DrawDebugCube(TransformSystem::GetWorldMatrix(*transform));
		}
	}
}

/// <summary>
/// BattleScene 内で使う仮 HUD 用 GameObject と数字テクスチャを初期化する。
/// </summary>
void BattleScene::InitializeBattleHUD()
{
	GameObjectId timerId = world.CreateTransform("BattleTimer");
	if (GameObject* timerObject = world.GetGameObject(timerId))
	{
		timerObject->tag = GameObjectTag::UI;
	}
	world.AddComponent<BattleTimerComponent>(timerId);

	GameObjectId player1GaugeId = world.CreateTransform("Player1HealthGauge");
	if (GameObject* player1GaugeObject = world.GetGameObject(player1GaugeId))
	{
		player1GaugeObject->tag = GameObjectTag::UI;
	}
	HealthGaugeComponent player1Gauge;
	player1Gauge.targetPlayerIndex = 0;
	world.AddComponent<HealthGaugeComponent>(player1GaugeId, player1Gauge);

	GameObjectId player2GaugeId = world.CreateTransform("Player2HealthGauge");
	if (GameObject* player2GaugeObject = world.GetGameObject(player2GaugeId))
	{
		player2GaugeObject->tag = GameObjectTag::UI;
	}
	HealthGaugeComponent player2Gauge;
	player2Gauge.targetPlayerIndex = 1;
	world.AddComponent<HealthGaugeComponent>(player2GaugeId, player2Gauge);

	hudNumberTexture = FindSceneTexture("assets/texture/number.png");
}

/// <summary>
/// 準備済みのバトルBGMをループ再生する。
/// </summary>
void BattleScene::InitializeBattleSounds()
{
	SoundManager& soundManager = SoundManager::GetInstance();
	soundManager.PlayBGM(SoundIds::BattleBgm01);
}

/// <summary>
/// Enter 直後に必要な生成反映とキャッシュ更新だけを実行する。
/// </summary>
void BattleScene::RunInitialWorldSetup()
{
	SpawnDestroySystem::Update(world);
	BattleCameraSystem::Update(world);
	BattleHUDSystem::Update(world, width, height);
	MotionSystem::Update(world);
	TransformSystem::UpdateWorldTransforms(world.GetGameObjects());

	if (world.HasActiveCamera())
	{
		CameraComponent& camera = world.GetActiveCamera();
		TransformComponent* cameraTransform = world.GetTransform(world.GetActiveCameraId());
		if (cameraTransform)
		{
			CameraSystem::Update(camera, *cameraTransform);
		}
	}
}

/// <summary>
/// BattleScene の HPバーとタイマーをゲームビューに重ねて描画する。
/// </summary>
void BattleScene::DrawBattleHUD(Renderer& renderer)
{
	BattleHUDSystem::Textures textures{};
	for (size_t index = 0; index < textures.size(); ++index)
	{
		textures[index] = FindSceneTexture(BattleHUDSystem::TexturePaths[index]);
	}

	BattleHUDSystem::Draw(renderer, world, width, height, hudNumberTexture, textures, visibleRoundPoints);
}

/// <summary>
/// 画面サイズ変更に合わせて BattleScene のメインカメラのアスペクト比を更新する。
/// </summary>
/// <param name="newWidth">新しい幅。</param>
/// <param name="newHeight">新しい高さ。</param>
void BattleScene::OnResize(int newWidth, int newHeight)
{
	if (newWidth <= 0 || newHeight <= 0)
	{
		return;
	}

	width = newWidth;
	height = newHeight;

	if (world.HasActiveCamera())
	{
		const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
		CameraSystem::SetAspectRatio(world.GetActiveCamera(), aspectRatio);

		BattleCameraSystem::Update(world);
		BattleHUDSystem::Update(world, width, height);
		TransformSystem::UpdateWorldTransforms(world.GetGameObjects());

		TransformComponent* cameraTransform = world.GetTransform(world.GetActiveCameraId());
		if (cameraTransform)
		{
			CameraSystem::Update(world.GetActiveCamera(), *cameraTransform);
		}
	}
}

/// <summary>
/// BattleScene が保持する World を取得する。
/// </summary>
/// <returns>変更可能な World。</returns>
World& BattleScene::GetWorld()
{
	return world;
}

/// <summary>
/// BattleScene が保持する World を読み取り専用で取得する。
/// </summary>
/// <returns>読み取り専用の World。</returns>
const World& BattleScene::GetWorld() const
{
	return world;
}

/// <summary>
/// 両者の決定が揃うまで結果画面を維持し、採用された操作で再戦または遷移する。
/// </summary>
void BattleScene::RunResultMenu()
{
	if (resultMenu.choice != BattleResultChoice::None)
	{
		return;
	}

	BattleResultSystem::UpdateMenu(setupData, resultMenu);
	switch (resultMenu.choice)
	{
	case BattleResultChoice::Rematch:
		Input::InputSystem::SetActionMap(Input::InputActionMapId::Gameplay);
		round = {};
		visibleRoundPoints = {};
		resultMenu = {};
		BuildRoundWorld();
		break;
	case BattleResultChoice::CharacterSelect:
		SceneManager::GetInstance().RequestChangeScene(std::make_unique<BattleSetupScene>(width, height, setupData));
		break;
	case BattleResultChoice::Title:
		SceneManager::GetInstance().RequestChangeScene(std::make_unique<TitleScene>(width, height));
		break;
	case BattleResultChoice::None:
		break;
	}
}

#if defined(_DEBUG)
/// <summary>
/// Debug 用 SceneView のカメラ、操作状態、RenderTexture を初期化する。
/// </summary>
void BattleScene::InitializeDebugSceneView()
{
	debugSceneCameraTransform = TransformComponent{};
	debugSceneCamera = CameraComponent{};
	debugSceneCameraControlState = DebugCameraControlState{};

	TransformSystem::SetLocalPosition(debugSceneCameraTransform, Vector3(0.0f, 8.0f, -20.0f));
	TransformSystem::SetLocalEulerRotationDegrees(debugSceneCameraTransform, Vector3(10.0f, 0.0f, 0.0f));
	TransformSystem::SetLocalScale(debugSceneCameraTransform, Vector3::One);
	TransformSystem::UpdateWorldTransform(debugSceneCameraTransform);

	const float aspectRatio = static_cast<float>(sceneViewWidth) / static_cast<float>(sceneViewHeight);
	CameraSystem::SetPerspective(debugSceneCamera, 45.0f, aspectRatio, 0.1f, 1000.0f);
	CameraSystem::Update(debugSceneCamera, debugSceneCameraTransform);

	const HRESULT hr = Renderer::CreateRenderTexture(sceneViewRenderTexture, sceneViewWidth, sceneViewHeight);
	if (FAILED(hr))
	{
		DebugLog("[SceneView] RenderTexture creation failed. hr=", static_cast<long>(hr));
	}
}

/// <summary>
/// SceneView 上にマウスがある時だけ、Debug 用カメラ操作と行列更新を行う。
/// </summary>
void BattleScene::UpdateDebugSceneViewCamera()
{
	DebugCameraControlSystem::Update(
		debugSceneCameraTransform,
		debugSceneCameraControlState,
		sceneViewHovered);

	TransformSystem::UpdateWorldTransform(debugSceneCameraTransform);
	CameraSystem::Update(debugSceneCamera, debugSceneCameraTransform);
}

/// <summary>
/// Debug 用カメラで World を RenderTexture に描画し、ImGui の SceneView に表示する。
/// </summary>
/// <param name="renderer">SceneView 描画に使用する Renderer。</param>
void BattleScene::DrawDebugSceneView(Renderer& renderer)
{
	if (!sceneViewRenderTexture.shaderResourceView)
	{
		sceneViewHovered = false;
		return;
	}

	const float clearColor[4] = { 0.05f, 0.05f, 0.08f, 1.0f };
	Renderer::BeginRenderTexture(sceneViewRenderTexture, clearColor);
	DrawWorldWithCamera(renderer, debugSceneCamera);
	Renderer::RestoreBackBuffer();

	sceneViewHovered = DebugImGuiSystem::DrawSceneView(
		sceneViewRenderTexture.shaderResourceView,
		sceneViewRenderTexture.width,
		sceneViewRenderTexture.height);
}

/// <summary>
/// Debug 表示が有効な時だけ、Player の PushBox / HurtBox / AttackBox を半透明 AABB で描画する。
/// </summary>
/// <param name="renderer">HitBox 描画に使用する Renderer。</param>
void BattleScene::DrawDebugHitBoxes(Renderer& renderer)
{
	if (!DebugImGuiSystem::ShouldDrawHitBoxes())
	{
		return;
	}

	constexpr float DebugBoxDepth = 0.08f;
	const Color pushBoxColor(1.0f, 1.0f, 1.0f, 0.28f);
	const Color hurtBoxColor(0.0f, 1.0f, 0.2f, 0.28f);
	const Color invincibleHurtBoxColor(1.0f, 0.9f, 0.0f, 0.30f);
	const Color attackBoxColor(1.0f, 0.0f, 0.0f, 0.32f);

	auto drawRect = [&renderer](const Vector3& basePosition, const HitBoxRect2D& rect, FacingDirection facingDirection, const Color& color)
	{
		if (!rect.enabled || rect.size.x <= 0.0f || rect.size.y <= 0.0f)
		{
			return;
		}

		const float facingSign = facingDirection == FacingDirection::Right ? 1.0f : -1.0f;
		const Vector3 center(
			basePosition.x + rect.offset.x * facingSign,
			basePosition.y + rect.offset.y,
			basePosition.z);

		const Matrix boxWorld =
			Matrix::CreateScale(rect.size.x * 0.5f, rect.size.y * 0.5f, DebugBoxDepth * 0.5f)
			* Matrix::CreateTranslation(center);
		renderer.DrawDebugBox(boxWorld, color);
	};

	auto findAssignedAttack = [](const CharacterAttackDataComponent& attackData, const std::string& slotId) -> const CharacterAssignedAttackData*
	{
		for (const CharacterAssignedAttackData& assignedAttack : attackData.attacks)
		{
			if (assignedAttack.slotId == slotId)
			{
				return &assignedAttack;
			}
		}

		return nullptr;
	};

	for (GameObject& object : world.GetGameObjects())
	{
		if (object.tag != GameObjectTag::Player)
		{
			continue;
		}

		const TransformComponent* transform = world.GetTransform(object.id);
		const StateComponent* state = world.GetComponent<StateComponent>(object.id);
		const HitBoxComponent* hitBox = world.GetComponent<HitBoxComponent>(object.id);
		if (!transform || !state || !hitBox)
		{
			continue;
		}

		const Vector3 basePosition = TransformSystem::GetWorldPosition(*transform);
		const Color& currentHurtBoxColor = state->isInvincible ? invincibleHurtBoxColor : hurtBoxColor;
		drawRect(basePosition, hitBox->hurtBox, state->facingDirection, currentHurtBoxColor);
		drawRect(basePosition, hitBox->pushBox, state->facingDirection, pushBoxColor);

		const bool isAttackState = state->currentActionState == PlayerActionState::GroundAttack
			|| state->currentActionState == PlayerActionState::AirAttack;
		if (!isAttackState || hitBox->currentAttack.slotId.empty())
		{
			continue;
		}

		const CharacterAttackDataComponent* characterAttackData = world.GetComponent<CharacterAttackDataComponent>(object.id);
		if (!characterAttackData)
		{
			continue;
		}

		const CharacterAssignedAttackData* assignedAttack = findAssignedAttack(*characterAttackData, hitBox->currentAttack.slotId);
		if (!assignedAttack)
		{
			continue;
		}

		if (!IsAttackFrameActive(assignedAttack->attack.frame, state->actionFrame))
		{
			continue;
		}

		for (const AttackHitboxData& attackHitbox : assignedAttack->attack.hitboxes)
		{
			if (attackHitbox.size.x <= 0.0f || attackHitbox.size.y <= 0.0f)
			{
				continue;
			}

			HitBoxRect2D attackRect;
			attackRect.offset = attackHitbox.offset;
			attackRect.size = attackHitbox.size;
			drawRect(basePosition, attackRect, state->facingDirection, attackBoxColor);
		}
	}
}
#endif
