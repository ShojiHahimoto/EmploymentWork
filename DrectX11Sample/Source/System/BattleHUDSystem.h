#pragma once

#include <d3d11.h>

struct BattleTimerComponent;
struct GameObject;
struct HealthGaugeComponent;
struct TransformComponent;
class World;

class BattleHUDSystem
{
public:
	/// <summary>
	/// UIタグを持つ GameObject を走査し、HUD用 Component ごとの表示状態を更新する。
	/// </summary>
	/// <param name="world">HUD GameObject と Player の HealthComponent を保持する World。</param>
	/// <param name="screenWidth">現在の描画幅。</param>
	/// <param name="screenHeight">現在の描画高さ。</param>
	static void Update(World& world, int screenWidth, int screenHeight);

	/// <summary>
	/// HPバーとラウンドタイマーをゲームビューへ 2D 描画する。
	/// </summary>
	/// <param name="world">BattleTimerComponent と HealthGaugeComponent を保持する World。</param>
	/// <param name="screenWidth">現在の描画幅。</param>
	/// <param name="screenHeight">現在の描画高さ。</param>
	/// <param name="numberTexture">数字スプライトシートの ShaderResourceView。</param>
	static void Draw(const World& world, int screenWidth, int screenHeight, ID3D11ShaderResourceView* numberTexture);

private:
	static void UpdateUIObject(World& world, GameObject& object, int screenWidth);
	static void UpdateHealthGauge(World& world, GameObject& object, HealthGaugeComponent& gauge, int screenWidth);

	static void DrawUIObject(const World& world, const GameObject& object, int screenWidth, ID3D11ShaderResourceView* numberTexture);
	static void DrawHealthGauge(const World& world, const GameObject& object, const HealthGaugeComponent& gauge);
	static void DrawTimer(const BattleTimerComponent& timer, int screenWidth, ID3D11ShaderResourceView* numberTexture);
};
