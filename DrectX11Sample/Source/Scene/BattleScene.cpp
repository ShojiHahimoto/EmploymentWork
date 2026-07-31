#include "Scene/BattleScene.h"

#include "Component/BattleTimerComponent.h"
#include "Component/CharacterAttackDataComponent.h"
#include "Component/HealthGaugeComponent.h"
#include "Component/HitBoxComponent.h"
#include "Component/ModelComponent.h"
#include "Component/StateComponent.h"
#include "Input/InputSystem.h"
#include "Resource/ModelResource.h"
#include "Scene/ResultScene.h"
#include "Scene/SceneManager.h"
#include "System/BattleHUDSystem.h"
#include "System/BattleResultSystem.h"
#include "System/CameraSystem.h"
#include "System/DebugCameraControlSystem.h"
#include "System/DebugImGuiSystem.h"
#include "System/Debugger.h"
#include "System/EmbedResolveSystem.h"
#include "System/HitCollisionSystem.h"
#include "System/HitResolveSystem.h"
#include "System/InputHistorySystem.h"
#include "System/MovementSystem.h"
#include "System/PlayerControlSystem.h"
#include "System/Renderer.h"
#include "System/SpawnDestroySystem.h"
#include "System/StateUpdateSystem.h"
#include "System/TransformSystem.h"

#include <algorithm>
#include <memory>

using namespace DirectX::SimpleMath;

/// <summary>
/// BattleScene を現在の描画サイズで初期化する。
/// </summary>
/// <param name="initialWidth">初期ウィンドウ幅。</param>
/// <param name="initialHeight">初期ウィンドウ高さ。</param>
BattleScene::BattleScene(int initialWidth, int initialHeight)
	: width(initialWidth)
	, height(initialHeight)
{
}

/// <summary>
/// バトル用 World、カメラ、初期モデル、デバッグ用オブジェクトを生成する。
/// </summary>
void BattleScene::Enter()
{
	Input::InputSystem::SetActionMap(Input::InputActionMapId::Gameplay);

	GameObjectId cameraId = world.CreateTransform("MainCamera");
	TransformComponent* cameraTransform = world.GetTransform(cameraId);
	if (cameraTransform)
	{
		TransformSystem::SetLocalPosition(*cameraTransform, Vector3(0.0f, 8.0f, -20.0f));
		TransformSystem::SetLocalEulerRotationDegrees(*cameraTransform, Vector3(0.0f, 0.0f, 0.0f));
	}

	ModelResourceManager::LoadModel(
		"Debugman",
		"assets/model/Debugman/Akai.fbx",
		Renderer::GetDevice());

	ModelResourceManager::LoadModel(
		"DebugPlayer",
		"assets/model/DebugPlayer/man.fbx",
		Renderer::GetDevice());

	ModelResourceManager::LoadModel(
		"DebugPlayer2",
		"assets/model/DebugPlayer2/woman.fbx",
		Renderer::GetDevice());

	world.RequestSpawn(
		SpawnType::DebugPlayer,
		"DebugPlayer",
		Vector3(-2.0f, 0.0f, 8.0f),
		Vector3(0.0f, 0.0f, 0.0f));

	world.RequestSpawn(
		SpawnType::DebugPlayer2,
		"DebugPlayer2",
		Vector3(2.0f, 0.0f, 8.0f),
		Vector3(0.0f, 0.0f, 0.0f));

	world.RequestSpawn(
		SpawnType::DebugCube,
		"DebugCube",
		Vector3(0.0f, 0.0f, 6.0f),
		Vector3(20.0f, 32.0f, 0.0f));

	InitializeBattleHUD();

	CameraComponent camera;
	const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
	CameraSystem::SetPerspective(camera, 45.0f, aspectRatio, 0.1f, 1000.0f);
	world.SetActiveCamera(cameraId, camera);

#if defined(_DEBUG)
	InitializeDebugSceneView();
#endif

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
	Renderer::ReleaseTexture(hudNumberTexture);
	world.Clear();
}

void BattleScene::RunSystems()
{
#if defined(_DEBUG)
	UpdateDebugSceneViewCamera();
#endif

	SpawnDestroySystem::Update(world);
	InputHistorySystem::Update(world);
	StateUpdateSystem::Update(world);
	PlayerControlSystem::Update(world);
	MovementSystem::Update(world);
	EmbedResolveSystem::Update(world);
	HitCollisionSystem::Update(world);
	HitResolveSystem::Update(world);
	BattleResultSystem::Update(world);
	BattleHUDSystem::Update(world, width, height);
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

	RequestResultSceneIfBattleFinished();
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
	DrawWorldWithCamera(renderer, camera);

#if defined(_DEBUG)
	DrawDebugSceneView(renderer);
	renderer.SetViewProjection(camera.viewMatrix, camera.projectionMatrix);
	DrawDebugHitBoxes(renderer);
#endif

	DrawBattleHUD();

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

	for (GameObject& object : world.GetGameObjects())
	{
		if (object.id == world.GetActiveCameraId())
		{
			continue;
		}

		if (object.tag == GameObjectTag::UI)
		{
			continue;
		}

		TransformComponent* transform = world.GetTransform(object.id);
		if (transform)
		{
			const ModelComponent* modelComponent = world.GetComponent<ModelComponent>(object.id);
			if (modelComponent)
			{
				const ModelResource* model = ModelResourceManager::GetModel(modelComponent->resourceKey);
				if (model)
				{
					if (renderer.DrawModel(*model, TransformSystem::GetWorldMatrix(*transform)))
					{
						continue;
					}
				}
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

	const HRESULT hr = Renderer::LoadTextureFromFile("assets/texture/number.png", &hudNumberTexture);
	if (FAILED(hr))
	{
		DebugLog("[BattleHUD] Number texture load failed. hr=", static_cast<long>(hr));
	}
}

/// <summary>
/// Enter 直後に必要な生成反映とキャッシュ更新だけを実行する。
/// </summary>
void BattleScene::RunInitialWorldSetup()
{
	SpawnDestroySystem::Update(world);
	BattleHUDSystem::Update(world, width, height);
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
void BattleScene::DrawBattleHUD()
{
	BattleHUDSystem::Draw(world, width, height, hudNumberTexture);
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
/// World に勝敗結果が記録されていれば、ResultScene への切り替えを予約する。
/// </summary>
void BattleScene::RequestResultSceneIfBattleFinished()
{
	if (!world.HasBattleResult())
	{
		return;
	}

	const BattleResult result = world.GetBattleResult();
	DebugLog("[BattleScene] Battle finished. Result=", static_cast<int>(result));

	SceneManager::GetInstance().RequestChangeScene(
		std::make_unique<ResultScene>(width, height, result));
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

	auto isAttackActive = [](const AttackFrameData& frame, int actionFrame)
	{
		const int activeStartFrame = std::max(0, frame.startup);
		const int activeFrameCount = std::max(0, frame.active);
		const int activeEndFrame = activeStartFrame + activeFrameCount;
		return activeFrameCount > 0
			&& actionFrame >= activeStartFrame
			&& actionFrame < activeEndFrame;
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
		drawRect(basePosition, hitBox->hurtBox, state->facingDirection, hurtBoxColor);
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

		if (!isAttackActive(assignedAttack->attack.frame, state->actionFrame))
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
