#include "System/BattleHUDSystem.h"

#include "Component/BattleTimerComponent.h"
#include "Component/HealthComponent.h"
#include "Component/HealthGaugeComponent.h"
#include "Component/StateComponent.h"
#include "Component/TransformComponent.h"
#include "Core/GameObject.h"
#include "System/Renderer.h"
#include "System/TransformSystem.h"
#include "World/World.h"

#include <SimpleMath.h>

#include <algorithm>
#include <string>

using namespace DirectX::SimpleMath;

namespace
{
	const Color GaugeBackColor(0.04f, 0.04f, 0.05f, 0.92f);
	const Color GaugeFrameColor(0.82f, 0.82f, 0.82f, 0.95f);
	const Color DamageGaugeColor(0.86f, 0.18f, 0.12f, 0.88f);
	const Color HealthGaugeColor(0.12f, 0.86f, 0.22f, 0.94f);

	/// <summary>
	/// float 座標から SpriteBatch 用の RECT を作成する。
	/// </summary>
	/// <param name="x">左上 X 座標。</param>
	/// <param name="y">左上 Y 座標。</param>
	/// <param name="width">矩形幅。</param>
	/// <param name="height">矩形高さ。</param>
	/// <returns>描画先 RECT。</returns>
	RECT MakeRect(float x, float y, float width, float height)
	{
		RECT rect = {};
		rect.left = static_cast<LONG>(x);
		rect.top = static_cast<LONG>(y);
		rect.right = static_cast<LONG>(x + width);
		rect.bottom = static_cast<LONG>(y + height);
		return rect;
	}

	/// <summary>
	/// HP と最大 HP から 0〜1 の表示割合を計算する。
	/// </summary>
	/// <param name="health">参照する HealthComponent。</param>
	/// <returns>HP 表示割合。</returns>
	float CalculateHealthRatio(const HealthComponent& health)
	{
		if (health.maxHp <= 0)
		{
			return 0.0f;
		}

		return std::clamp(
			static_cast<float>(health.currentHp) / static_cast<float>(health.maxHp),
			0.0f,
			1.0f);
	}

	/// <summary>
	/// 指定 Player が、HPバーのダメージ蓄積表示を止める硬直中か確認する。
	/// </summary>
	/// <param name="world">StateComponent を取得する World。</param>
	/// <param name="playerId">確認する Player GameObject ID。</param>
	/// <returns>被弾・ガード・ダウン系の硬直中なら true。</returns>
	bool IsPlayerInDamageHoldState(const World& world, GameObjectId playerId)
	{
		const StateComponent* state = world.GetComponent<StateComponent>(playerId);
		return state
			&& (state->currentActionState == PlayerActionState::Hitstun
				|| state->currentActionState == PlayerActionState::Guardstun
				|| state->currentActionState == PlayerActionState::AirHitstun
				|| state->currentActionState == PlayerActionState::Down
				|| state->currentActionState == PlayerActionState::WakeUp);
	}

	/// <summary>
	/// タイマー残りフレームから表示用の秒数を作る。
	/// </summary>
	/// <param name="timer">参照する BattleTimerComponent。</param>
	/// <returns>0〜99 に丸めた表示秒数。</returns>
	int CalculateDisplaySeconds(const BattleTimerComponent& timer)
	{
		const int remainingFrames = std::max(0, timer.remainingFrames);
		const int seconds = (remainingFrames + 59) / 60;
		return std::clamp(seconds, 0, 99);
	}
}

void BattleHUDSystem::Update(World& world, int screenWidth, int screenHeight)
{
	(void)screenHeight;

	for (GameObject& object : world.GetGameObjects())
	{
		if (object.tag != GameObjectTag::UI)
		{
			continue;
		}

		UpdateUIObject(world, object, screenWidth);
	}
}

void BattleHUDSystem::Draw(const World& world, int screenWidth, int screenHeight, ID3D11ShaderResourceView* numberTexture)
{
	(void)screenHeight;

	for (const GameObject& object : world.GetGameObjects())
	{
		if (object.tag != GameObjectTag::UI)
		{
			continue;
		}

		DrawUIObject(world, object, screenWidth, numberTexture);
	}
}

/// <summary>
/// UIタグを持つ 1 つの GameObject に対して、保持 Component に応じた更新処理を呼ぶ。
/// </summary>
/// <param name="world">Component を取得する World。</param>
/// <param name="object">更新対象の UI GameObject。</param>
/// <param name="screenWidth">現在の描画幅。</param>
void BattleHUDSystem::UpdateUIObject(World& world, GameObject& object, int screenWidth)
{
	if (HealthGaugeComponent* gauge = world.GetComponent<HealthGaugeComponent>(object.id))
	{
		UpdateHealthGauge(world, object, *gauge, screenWidth);
	}

	// BattleTimerComponent は BattleResultSystem が残りフレームを更新する。
	// HUD側では描画時に読むだけなので、ここでは更新処理を持たせない。
}

/// <summary>
/// HealthGaugeComponent の表示割合と画面上の配置を更新する。
/// </summary>
/// <param name="world">Player の HealthComponent と StateComponent を保持する World。</param>
/// <param name="object">HealthGaugeComponent を持つ UI GameObject。</param>
/// <param name="gauge">更新する HealthGaugeComponent。</param>
/// <param name="screenWidth">現在の描画幅。</param>
void BattleHUDSystem::UpdateHealthGauge(World& world, GameObject& object, HealthGaugeComponent& gauge, int screenWidth)
{
	TransformComponent* transform = world.GetTransform(object.id);
	if (!transform)
	{
		return;
	}

	const GameObjectId targetPlayerId = world.GetBattlePlayerId(gauge.targetPlayerIndex);
	const HealthComponent* health = world.GetComponent<HealthComponent>(targetPlayerId);
	if (!health)
	{
		return;
	}

	const float currentRatio = CalculateHealthRatio(*health);
	if (!gauge.initialized)
	{
		gauge.healthRatio = currentRatio;
		gauge.damageRatio = currentRatio;
		gauge.initialized = true;
	}
	else
	{
		gauge.healthRatio = currentRatio;
		if (IsPlayerInDamageHoldState(world, targetPlayerId))
		{
			// ヒットスタン/ガードスタン中はダメージバーを古い長さで止め、連続ダメージの蓄積を見せる。
			gauge.damageRatio = std::max(gauge.damageRatio, gauge.healthRatio);
		}
		else
		{
			gauge.damageRatio = gauge.healthRatio;
		}
	}

	const float x = gauge.targetPlayerIndex == 0
		? gauge.horizontalMargin
		: static_cast<float>(screenWidth) - gauge.horizontalMargin - gauge.maxWidth;
	const Vector3 position(x, gauge.top, 0.0f);
	const Vector3 scale(gauge.maxWidth, gauge.height, 1.0f);
	TransformSystem::SetLocalPosition(*transform, position);
	TransformSystem::SetLocalScale(*transform, scale);
}

/// <summary>
/// UIタグを持つ 1 つの GameObject に対して、保持 Component に応じた描画処理を呼ぶ。
/// </summary>
/// <param name="world">Component と Transform を取得する World。</param>
/// <param name="object">描画対象の UI GameObject。</param>
/// <param name="screenWidth">現在の描画幅。</param>
/// <param name="numberTexture">数字スプライトシートの ShaderResourceView。</param>
void BattleHUDSystem::DrawUIObject(const World& world, const GameObject& object, int screenWidth, ID3D11ShaderResourceView* numberTexture)
{
	if (const HealthGaugeComponent* gauge = world.GetComponent<HealthGaugeComponent>(object.id))
	{
		DrawHealthGauge(world, object, *gauge);
	}

	if (const BattleTimerComponent* timer = world.GetComponent<BattleTimerComponent>(object.id))
	{
		DrawTimer(*timer, screenWidth, numberTexture);
	}
}

/// <summary>
/// HealthGaugeComponent を持つ UI GameObject を 2D 矩形として描画する。
/// </summary>
/// <param name="world">TransformComponent を取得する World。</param>
/// <param name="object">描画対象の UI GameObject。</param>
/// <param name="gauge">描画する HealthGaugeComponent。</param>
void BattleHUDSystem::DrawHealthGauge(const World& world, const GameObject& object, const HealthGaugeComponent& gauge)
{
	const TransformComponent* transform = world.GetTransform(object.id);
	if (!transform)
	{
		return;
	}

	const Vector3& position = TransformSystem::GetLocalPosition(*transform);
	const Vector3& scale = TransformSystem::GetLocalScale(*transform);
	if (scale.x <= 0.0f || scale.y <= 0.0f)
	{
		return;
	}

	Renderer::DrawScreenRect(MakeRect(position.x - 2.0f, position.y - 2.0f, scale.x + 4.0f, scale.y + 4.0f), GaugeFrameColor);
	Renderer::DrawScreenRect(MakeRect(position.x, position.y, scale.x, scale.y), GaugeBackColor);
	Renderer::DrawScreenRect(MakeRect(position.x, position.y, scale.x * gauge.damageRatio, scale.y), DamageGaugeColor);
	Renderer::DrawScreenRect(MakeRect(position.x, position.y, scale.x * gauge.healthRatio, scale.y), HealthGaugeColor);
}

/// <summary>
/// number.png の 0〜9 スプライトシートから残り時間を切り出して描画する。
/// </summary>
/// <param name="timer">表示秒数の元にする BattleTimerComponent。</param>
/// <param name="screenWidth">現在の描画幅。</param>
/// <param name="numberTexture">数字スプライトシートの ShaderResourceView。</param>
void BattleHUDSystem::DrawTimer(const BattleTimerComponent& timer, int screenWidth, ID3D11ShaderResourceView* numberTexture)
{
	if (!numberTexture)
	{
		return;
	}

	int textureWidth = 0;
	int textureHeight = 0;
	if (!Renderer::GetTextureSize(numberTexture, textureWidth, textureHeight) || textureWidth <= 0 || textureHeight <= 0)
	{
		return;
	}

	const int digitCountInTexture = 10;
	const int sourceDigitWidth = textureWidth / digitCountInTexture;
	const int sourceDigitHeight = textureHeight;
	if (sourceDigitWidth <= 0)
	{
		return;
	}

	const int displaySeconds = CalculateDisplaySeconds(timer);
	const std::string digits = displaySeconds < 10
		? "0" + std::to_string(displaySeconds)
		: std::to_string(displaySeconds);

	const float digitWidth = 40.0f;
	const float digitHeight = 44.0f;
	const float gap = 2.0f;
	const float totalWidth = digitWidth * static_cast<float>(digits.size())
		+ gap * static_cast<float>(digits.size() - 1);
	const float startX = static_cast<float>(screenWidth) * 0.5f - totalWidth * 0.5f;
	const float top = 20.0f;

	for (size_t index = 0; index < digits.size(); ++index)
	{
		const int digit = digits[index] - '0';
		if (digit < 0 || digit > 9)
		{
			continue;
		}

		RECT source = {};
		source.left = sourceDigitWidth * digit;
		source.top = 0;
		source.right = source.left + sourceDigitWidth;
		source.bottom = sourceDigitHeight;

		const float x = startX + (digitWidth + gap) * static_cast<float>(index);
		Renderer::DrawTextureRegion(numberTexture, MakeRect(x, top, digitWidth, digitHeight), source, Color(1.0f, 1.0f, 1.0f, 1.0f));
	}
}
