#include "System/BattleHUDSystem.h"

#include "Component/BattleTimerComponent.h"
#include "Component/HealthComponent.h"
#include "Component/HealthGaugeComponent.h"
#include "Component/InputHistoryComponent.h"
#include "Component/StateComponent.h"
#include "Component/TransformComponent.h"
#include "Core/GameObject.h"
#include "Core/Renderer.h"
#include "System/TransformSystem.h"
#include "Core/World.h"

#include "UI/UIRenderer.h"

#include <SimpleMath.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

using namespace DirectX::SimpleMath;

namespace
{
	const Color InputHistoryBackColor(0.02f, 0.02f, 0.025f, 0.58f);
	const Color InputHistoryFrameColor(0.95f, 0.95f, 0.95f, 0.88f);
	const Color InputHistoryDirectionColor(1.0f, 0.92f, 0.28f, 1.0f);

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
				|| state->currentActionState == PlayerActionState::StandGuardstun
				|| state->currentActionState == PlayerActionState::CrouchGuardstun
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

	void DrawImage(ID3D11ShaderResourceView* texture, const RECT& destination, bool flip = false)
	{
		int width = 0;
		int height = 0;
		Renderer::GetTextureSize(texture, width, height);
		Renderer::DrawTextureRegion(texture, destination, {0, 0, width, height}, Color(1, 1, 1, 1), flip);
	}

	/// <summary>HP素材の斜めの内枠を基準に、各行の残量を描画する。</summary>
	void DrawHealthFill(ID3D11ShaderResourceView* texture, const RECT& bar, bool flip, float ratio)
	{
		if (ratio <= 0.0f || bar.bottom <= bar.top || bar.right <= bar.left)
		{
			return;
		}

		// HP fill PNGs share the same 768 x 48 trapezoid and vertical gradient.
		constexpr float assetWidth = 768.0f;
		constexpr float assetHeight = 48.0f;
		const float barWidth = static_cast<float>(bar.right - bar.left);
		const float barHeight = static_cast<float>(bar.bottom - bar.top);
		for (long y = bar.top; y < bar.bottom; ++y)
		{
			const float sourceY = (static_cast<float>(y - bar.top) + 0.5f) * assetHeight / barHeight;
			if (sourceY < 6.0f || sourceY >= 40.0f)
			{
				continue;
			}

			const float t = (sourceY - 6.0f) / 34.0f;
			const float innerStart = (9.0f + 22.0f * t) * barWidth / assetWidth;
			const float innerEnd = (736.0f + 21.0f * t) * barWidth / assetWidth;
			const long fullStart = static_cast<long>(std::lround(innerStart));
			const long end = std::max(fullStart + 1, static_cast<long>(std::lround(innerEnd)));
			const long start = std::clamp(
				static_cast<long>(std::lround(std::lerp(innerEnd, innerStart, ratio))), fullStart, end - 1);
			const long sampleY = static_cast<long>(sourceY);
			const RECT destination = flip ? RECT{bar.right - end, y, bar.right - start, y + 1}
				: RECT{bar.left + start, y, bar.left + end, y + 1};
			Renderer::DrawTextureRegion(texture, destination, {384, sampleY, 385, sampleY + 1}, Color(1, 1, 1, 1));
		}
	}

	void DrawAttackButtons(uint32_t mask, float x, float y, const BattleHUDSystem::Textures& textures)
	{
		constexpr uint32_t masks[] = {InputHistoryAttackMask::AttackA, InputHistoryAttackMask::AttackB,
			InputHistoryAttackMask::AttackX, InputHistoryAttackMask::AttackY};
		for (size_t index = 0; index < std::size(masks); ++index)
		{
			if (mask & masks[index])
			{
				DrawImage(textures[9 + index], MakeRect(x, y, 20, 20));
				x += 24.0f;
			}
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
		const BattleHUDSystem::Textures& textures,
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

		DrawAttackButtons(entry.attackPressMask, currentX, y + 2.0f, textures);
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
		float startY,
		const BattleHUDSystem::Textures& textures,
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
				textures,
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
	void DrawInputHistories(const World& world, ID3D11ShaderResourceView* numberTexture,
		const BattleHUDSystem::Textures& textures, float startY)
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
			startY,
			textures,
			numberTexture,
			sourceDigitWidth,
			sourceDigitHeight);
	}
}

/// <summary>UIタグを持つ GameObject を走査し、HUD用 Component ごとの表示状態を更新する。</summary>
/// <param name="world">HUD GameObject と Player の HealthComponent を保持する World。</param>
/// <param name="screenWidth">現在の描画幅。</param>
/// <param name="screenHeight">現在の描画高さ。</param>
/// <param name="holdDamage">被弾状態に応じてダメージ表示を保持する場合は true。</param>
void BattleHUDSystem::Update(World& world, int screenWidth, int screenHeight, bool holdDamage)
{
	(void)screenHeight;

	for (GameObject& object : world.GetGameObjects())
	{
		if (object.tag != GameObjectTag::UI)
		{
			continue;
		}

		UpdateUIObject(world, object, screenWidth, holdDamage);
	}
}

/// <summary>対戦HUDと入力履歴を描画する。</summary>
void BattleHUDSystem::Draw(Renderer& renderer, const World& world, int screenWidth, int screenHeight,
	ID3D11ShaderResourceView* numberTexture, const Textures& textures, const std::array<int, 2>& roundPoints)
{
	const float width = static_cast<float>(screenWidth);
	const float scale = std::min(width / 1600.0f, screenHeight / 900.0f);
	const float center = width * 0.5f;
	const float timerHalf = 64.0f * scale;
	const float barInset = 86.0f * scale;
	const float barWidth = center - timerHalf - barInset;
	std::vector<GameUI::DrawCommand> labels;

	for (const auto& object : world.GetGameObjects())
	{
		if (const auto* gauge = world.GetComponent<HealthGaugeComponent>(object.id))
		{
			const bool right = gauge->targetPlayerIndex == 1;
			const float barX = right ? center + timerHalf : barInset;
			const RECT bar = MakeRect(barX, 53.0f * scale, barWidth, 47.0f * scale);
			DrawImage(textures[0], bar, right);
			DrawHealthFill(textures[3], bar, right, gauge->damageRatio);
			DrawHealthFill(textures[right ? 2 : 1], bar, right, gauge->healthRatio);

			const float plateWidth = 270.0f * scale;
			DrawImage(textures[right ? 13 : 4], MakeRect(right ? width - plateWidth : 0.0f,
				18.0f * scale, plateWidth, 80.0f * scale));
			DrawImage(textures[right ? 6 : 5], MakeRect(right ? width - 90.0f * scale : 14.0f * scale,
				20.0f * scale, 76.0f * scale, 76.0f * scale));

			GameUI::DrawCommand label;
			label.type = GameUI::DrawCommandType::Text;
			label.bounds = MakeRect(right ? width - 247.0f * scale : 87.0f * scale,
				18.0f * scale, 160.0f * scale, 36.0f * scale);
			label.text = right ? L"Player 2" : L"Player 1";
			label.fontSize = 24.0f * scale;
			label.bold = true;
			label.horizontalAlignment = GameUI::TextHorizontalAlignment::Center;
			labels.push_back(label);

			for (int mark = 0; mark < 2; ++mark)
			{
				const float inset = (25.0f + mark * 34.0f) * scale;
				const float x = right ? center + timerHalf + inset : center - timerHalf - inset - 30.0f * scale;
				const int textureIndex = mark < roundPoints[right ? 1 : 0] ? 14 : 7;
				DrawImage(textures[textureIndex], MakeRect(x, 19.0f * scale, 30.0f * scale, 30.0f * scale));
			}
		}

		if (const auto* timer = world.GetComponent<BattleTimerComponent>(object.id))
		{
			const int seconds = CalculateDisplaySeconds(*timer);
			const std::string digits = seconds < 10 ? "0" + std::to_string(seconds) : std::to_string(seconds);
			const float digitWidth = 61.0f * scale;
			float x = center - digitWidth * static_cast<float>(digits.size()) * 0.5f;
			for (const char digit : digits)
			{
				DrawNumberDigit(textures[8], 128, 144, digit - '0', x, 33.0f * scale,
					digitWidth, 69.0f * scale, Color(1, 1, 1, 1));
				x += digitWidth;
			}
		}
	}

	DrawInputHistories(world, numberTexture, textures, std::max(92.0f, 116.0f * scale));
	GameUI::UIRenderer::Draw(renderer, labels);
}

/// <summary>
/// UIタグを持つ 1 つの GameObject に対して、保持 Component に応じた更新処理を呼ぶ。
/// </summary>
/// <param name="world">Component を取得する World。</param>
/// <param name="object">更新対象の UI GameObject。</param>
/// <param name="screenWidth">現在の描画幅。</param>
/// <param name="holdDamage">ダメージ表示の保持を許可する場合は true。</param>
void BattleHUDSystem::UpdateUIObject(World& world, GameObject& object, int screenWidth, bool holdDamage)
{
	if (HealthGaugeComponent* gauge = world.GetComponent<HealthGaugeComponent>(object.id))
	{
		UpdateHealthGauge(world, object, *gauge, screenWidth, holdDamage);
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
/// <param name="holdDamage">false の場合、被弾状態に関係なくダメージ表示を現在HPに揃える。</param>
void BattleHUDSystem::UpdateHealthGauge(World& world, GameObject& object, HealthGaugeComponent& gauge, int screenWidth,
	bool holdDamage)
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
		if (holdDamage && IsPlayerInDamageHoldState(world, targetPlayerId))
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
