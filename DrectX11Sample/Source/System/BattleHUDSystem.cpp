#include "System/BattleHUDSystem.h"

#include "Component/BattleTimerComponent.h"
#include "Component/HealthComponent.h"
#include "Component/HealthGaugeComponent.h"
#include "Component/InputHistoryComponent.h"
#include "Component/StateComponent.h"
#include "Component/TransformComponent.h"
#include "Core/GameObject.h"
#include "System/Renderer.h"
#include "System/TransformSystem.h"
#include "World/World.h"

#include <SimpleMath.h>

#include <algorithm>
#include <cstdint>
#include <string>

using namespace DirectX::SimpleMath;

namespace
{
	const Color GaugeBackColor(0.04f, 0.04f, 0.05f, 0.92f);
	const Color GaugeFrameColor(0.82f, 0.82f, 0.82f, 0.95f);
	const Color DamageGaugeColor(0.86f, 0.18f, 0.12f, 0.88f);
	const Color HealthGaugeColor(0.12f, 0.86f, 0.22f, 0.94f);
	const Color InputHistoryBackColor(0.02f, 0.02f, 0.025f, 0.58f);
	const Color InputHistoryFrameColor(0.95f, 0.95f, 0.95f, 0.88f);
	const Color InputHistoryDirectionColor(1.0f, 0.92f, 0.28f, 1.0f);
	const Color InputHistoryTextColor(1.0f, 1.0f, 1.0f, 1.0f);
	const Color InputHistoryButtonAColor(0.15f, 0.74f, 0.32f, 0.94f);
	const Color InputHistoryButtonBColor(0.20f, 0.52f, 1.0f, 0.94f);
	const Color InputHistoryButtonXColor(0.78f, 0.38f, 1.0f, 0.94f);
	const Color InputHistoryButtonYColor(1.0f, 0.58f, 0.16f, 0.94f);

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
				|| state->currentActionState == PlayerActionState::Down);
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

	/// <summary>
	/// number.png の切り出しに使う 1 桁分の元サイズを取得する。
	/// </summary>
	/// <param name="numberTexture">数字スプライトシートの ShaderResourceView。</param>
	/// <param name="outSourceDigitWidth">1 桁分の元幅。</param>
	/// <param name="outSourceDigitHeight">1 桁分の元高さ。</param>
	/// <returns>数字スプライトシートとして扱える場合は true。</returns>
	bool TryGetNumberTextureMetrics(
		ID3D11ShaderResourceView* numberTexture,
		int& outSourceDigitWidth,
		int& outSourceDigitHeight)
	{
		int textureWidth = 0;
		int textureHeight = 0;
		if (!Renderer::GetTextureSize(numberTexture, textureWidth, textureHeight) || textureWidth <= 0 || textureHeight <= 0)
		{
			return false;
		}

		constexpr int digitCountInTexture = 10;
		outSourceDigitWidth = textureWidth / digitCountInTexture;
		outSourceDigitHeight = textureHeight;
		return outSourceDigitWidth > 0 && outSourceDigitHeight > 0;
	}

	/// <summary>
	/// number.png から 1 桁の数字を切り出して描画する。
	/// </summary>
	/// <param name="numberTexture">数字スプライトシートの ShaderResourceView。</param>
	/// <param name="sourceDigitWidth">1 桁分の元幅。</param>
	/// <param name="sourceDigitHeight">1 桁分の元高さ。</param>
	/// <param name="digit">描画する 0〜9 の数字。</param>
	/// <param name="x">描画先左上 X。</param>
	/// <param name="y">描画先左上 Y。</param>
	/// <param name="width">描画幅。</param>
	/// <param name="height">描画高さ。</param>
	/// <param name="color">乗算色。</param>
	void DrawNumberDigit(
		ID3D11ShaderResourceView* numberTexture,
		int sourceDigitWidth,
		int sourceDigitHeight,
		int digit,
		float x,
		float y,
		float width,
		float height,
		const Color& color)
	{
		if (digit < 0 || digit > 9)
		{
			return;
		}

		RECT source = {};
		source.left = sourceDigitWidth * digit;
		source.top = 0;
		source.right = source.left + sourceDigitWidth;
		source.bottom = sourceDigitHeight;
		Renderer::DrawTextureRegion(numberTexture, MakeRect(x, y, width, height), source, color);
	}

	/// <summary>
	/// 正の整数を number.png の数字で左から描画する。
	/// </summary>
	/// <param name="numberTexture">数字スプライトシートの ShaderResourceView。</param>
	/// <param name="sourceDigitWidth">1 桁分の元幅。</param>
	/// <param name="sourceDigitHeight">1 桁分の元高さ。</param>
	/// <param name="value">描画する整数。</param>
	/// <param name="x">描画開始 X。</param>
	/// <param name="y">描画開始 Y。</param>
	/// <param name="digitWidth">1 桁の描画幅。</param>
	/// <param name="digitHeight">1 桁の描画高さ。</param>
	/// <param name="gap">桁間の余白。</param>
	/// <param name="color">乗算色。</param>
	/// <returns>描画後の右端 X。</returns>
	float DrawInteger(
		ID3D11ShaderResourceView* numberTexture,
		int sourceDigitWidth,
		int sourceDigitHeight,
		int value,
		float x,
		float y,
		float digitWidth,
		float digitHeight,
		float gap,
		const Color& color)
	{
		const std::string text = std::to_string(std::max(0, value));
		float currentX = x;
		for (char character : text)
		{
			DrawNumberDigit(
				numberTexture,
				sourceDigitWidth,
				sourceDigitHeight,
				character - '0',
				currentX,
				y,
				digitWidth,
				digitHeight,
				color);
			currentX += digitWidth + gap;
		}

		return currentX;
	}

	/// <summary>
	/// 入力表示用の 5x7 ブロック文字パターンを取得する。
	/// </summary>
	/// <param name="glyph">取得する文字。</param>
	/// <param name="row">0〜6 の行番号。</param>
	/// <returns>5文字分の 0/1 パターン。</returns>
	const char* GetBlockGlyphRow(char glyph, int row)
	{
		static const char* blank[] =
		{
			"00000",
			"00000",
			"00000",
			"00000",
			"00000",
			"00000",
			"00000",
		};
		static const char* glyphA[] =
		{
			"01110",
			"10001",
			"10001",
			"11111",
			"10001",
			"10001",
			"10001",
		};
		static const char* glyphB[] =
		{
			"11110",
			"10001",
			"10001",
			"11110",
			"10001",
			"10001",
			"11110",
		};
		static const char* glyphF[] =
		{
			"11111",
			"10000",
			"10000",
			"11110",
			"10000",
			"10000",
			"10000",
		};
		static const char* glyphX[] =
		{
			"10001",
			"01010",
			"00100",
			"00100",
			"01010",
			"10001",
			"10001",
		};
		static const char* glyphY[] =
		{
			"10001",
			"01010",
			"00100",
			"00100",
			"00100",
			"00100",
			"00100",
		};

		const char** selectedGlyph = blank;
		switch (glyph)
		{
		case 'A':
			selectedGlyph = glyphA;
			break;
		case 'B':
			selectedGlyph = glyphB;
			break;
		case 'F':
			selectedGlyph = glyphF;
			break;
		case 'X':
			selectedGlyph = glyphX;
			break;
		case 'Y':
			selectedGlyph = glyphY;
			break;
		default:
			selectedGlyph = blank;
			break;
		}

		return selectedGlyph[std::clamp(row, 0, 6)];
	}

	/// <summary>
	/// 5x7 ブロック文字を単色矩形の集合で描画する。
	/// </summary>
	/// <param name="glyph">描画する文字。</param>
	/// <param name="x">描画先左上 X。</param>
	/// <param name="y">描画先左上 Y。</param>
	/// <param name="cellSize">1 ブロックのサイズ。</param>
	/// <param name="color">文字色。</param>
	void DrawBlockGlyph(char glyph, float x, float y, float cellSize, const Color& color)
	{
		for (int row = 0; row < 7; ++row)
		{
			const char* rowPattern = GetBlockGlyphRow(glyph, row);
			for (int column = 0; column < 5; ++column)
			{
				if (rowPattern[column] != '1')
				{
					continue;
				}

				Renderer::DrawScreenRect(
					MakeRect(
						x + static_cast<float>(column) * cellSize,
						y + static_cast<float>(row) * cellSize,
						cellSize,
						cellSize),
					color);
			}
		}
	}

	/// <summary>
	/// 攻撃ボタン 1 つ分の背景と A/B/X/Y ラベルを描画する。
	/// </summary>
	/// <param name="label">表示するボタン名。</param>
	/// <param name="x">描画先左上 X。</param>
	/// <param name="y">描画先左上 Y。</param>
	/// <param name="backgroundColor">ボタン背景色。</param>
	void DrawAttackButtonLabel(char label, float x, float y, const Color& backgroundColor)
	{
		constexpr float buttonSize = 20.0f;
		constexpr float glyphCellSize = 2.0f;
		constexpr float glyphWidth = 5.0f * glyphCellSize;
		constexpr float glyphHeight = 7.0f * glyphCellSize;

		Renderer::DrawScreenRect(MakeRect(x, y, buttonSize, buttonSize), backgroundColor);
		DrawBlockGlyph(
			label,
			x + (buttonSize - glyphWidth) * 0.5f,
			y + (buttonSize - glyphHeight) * 0.5f,
			glyphCellSize,
			InputHistoryTextColor);
	}

	/// <summary>
	/// 押されている攻撃ボタン群を A/B/X/Y の順に描画する。
	/// </summary>
	/// <param name="attackPressMask">InputHistoryAttackMask の Press ビットマスク。</param>
	/// <param name="x">描画開始 X。描画した分だけ右へ進める。</param>
	/// <param name="y">描画先左上 Y。</param>
	void DrawAttackButtons(uint32_t attackPressMask, float& x, float y)
	{
		constexpr float buttonGap = 4.0f;
		constexpr float buttonAdvance = 20.0f + buttonGap;

		if (attackPressMask & InputHistoryAttackMask::AttackA)
		{
			DrawAttackButtonLabel('A', x, y, InputHistoryButtonAColor);
			x += buttonAdvance;
		}
		if (attackPressMask & InputHistoryAttackMask::AttackB)
		{
			DrawAttackButtonLabel('B', x, y, InputHistoryButtonBColor);
			x += buttonAdvance;
		}
		if (attackPressMask & InputHistoryAttackMask::AttackX)
		{
			DrawAttackButtonLabel('X', x, y, InputHistoryButtonXColor);
			x += buttonAdvance;
		}
		if (attackPressMask & InputHistoryAttackMask::AttackY)
		{
			DrawAttackButtonLabel('Y', x, y, InputHistoryButtonYColor);
			x += buttonAdvance;
		}
	}

	/// <summary>
	/// 入力表示履歴の 1 行を描画する。
	/// </summary>
	/// <param name="entry">描画する圧縮入力履歴。</param>
	/// <param name="numberTexture">数字スプライトシートの ShaderResourceView。</param>
	/// <param name="sourceDigitWidth">1 桁分の元幅。</param>
	/// <param name="sourceDigitHeight">1 桁分の元高さ。</param>
	/// <param name="x">行の左上 X。</param>
	/// <param name="y">行の左上 Y。</param>
	void DrawInputHistoryEntry(
		const InputDisplayHistoryEntry& entry,
		ID3D11ShaderResourceView* numberTexture,
		int sourceDigitWidth,
		int sourceDigitHeight,
		float x,
		float y)
	{
		constexpr float rowWidth = 168.0f;
		constexpr float rowHeight = 24.0f;
		constexpr float digitWidth = 12.0f;
		constexpr float digitHeight = 17.0f;
		constexpr float digitGap = 1.0f;

		Renderer::DrawScreenRect(MakeRect(x, y, rowWidth, rowHeight), InputHistoryBackColor);

		float currentX = x + 6.0f;
		const float contentY = y + 3.0f;
		currentX = DrawInteger(
			numberTexture,
			sourceDigitWidth,
			sourceDigitHeight,
			std::clamp(entry.holdFrames, 1, InputHistoryComponent::MaxDisplayHoldFrames),
			currentX,
			contentY,
			digitWidth,
			digitHeight,
			digitGap,
			InputHistoryFrameColor);

		DrawBlockGlyph('F', currentX + 2.0f, y + 5.0f, 1.6f, InputHistoryFrameColor);
		currentX += 16.0f;

		DrawNumberDigit(
			numberTexture,
			sourceDigitWidth,
			sourceDigitHeight,
			std::clamp(entry.direction, 1, 9),
			currentX,
			contentY,
			digitWidth,
			digitHeight,
			InputHistoryDirectionColor);
		currentX += 22.0f;

		DrawAttackButtons(entry.attackPressMask, currentX, y + 2.0f);
	}

	/// <summary>
	/// 指定 Player の入力表示履歴をゲームビュー上に描画する。
	/// </summary>
	/// <param name="world">Player と InputHistoryComponent を保持する World。</param>
	/// <param name="playerIndex">表示対象の Player 番号。</param>
	/// <param name="numberTexture">数字スプライトシートの ShaderResourceView。</param>
	/// <param name="sourceDigitWidth">1 桁分の元幅。</param>
	/// <param name="sourceDigitHeight">1 桁分の元高さ。</param>
	void DrawInputHistoryForPlayer(
		const World& world,
		int playerIndex,
		ID3D11ShaderResourceView* numberTexture,
		int sourceDigitWidth,
		int sourceDigitHeight)
	{
		const GameObjectId playerId = world.GetBattlePlayerId(playerIndex);
		const InputHistoryComponent* inputHistory = world.GetComponent<InputHistoryComponent>(playerId);
		if (!inputHistory || inputHistory->displayEntryCount <= 0)
		{
			return;
		}

		constexpr float startX = 24.0f;
		constexpr float startY = 92.0f;
		constexpr float rowGap = 4.0f;
		constexpr float rowHeight = 24.0f;
		const int entryCount = std::min(
			inputHistory->displayEntryCount,
			InputHistoryComponent::DisplayHistoryEntryCount);

		for (int index = 0; index < entryCount; ++index)
		{
			const float y = startY + static_cast<float>(index) * (rowHeight + rowGap);
			DrawInputHistoryEntry(
				inputHistory->displayEntries[static_cast<size_t>(index)],
				numberTexture,
				sourceDigitWidth,
				sourceDigitHeight,
				startX,
				y);
		}
	}

	/// <summary>
	/// 入力履歴表示を描画する。現段階では 1P のみ左側に表示する。
	/// </summary>
	/// <param name="world">Player と InputHistoryComponent を保持する World。</param>
	/// <param name="numberTexture">数字スプライトシートの ShaderResourceView。</param>
	void DrawInputHistories(const World& world, ID3D11ShaderResourceView* numberTexture)
	{
		int sourceDigitWidth = 0;
		int sourceDigitHeight = 0;
		if (!TryGetNumberTextureMetrics(numberTexture, sourceDigitWidth, sourceDigitHeight))
		{
			return;
		}

		DrawInputHistoryForPlayer(
			world,
			0,
			numberTexture,
			sourceDigitWidth,
			sourceDigitHeight);
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

	DrawInputHistories(world, numberTexture);
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
