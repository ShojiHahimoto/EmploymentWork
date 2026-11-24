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
	static void Update(World& world, int screenWidth, int screenHeight);

	static void Draw(const World& world, int screenWidth, int screenHeight, ID3D11ShaderResourceView* numberTexture);

private:
	static void UpdateUIObject(World& world, GameObject& object, int screenWidth);
	static void UpdateHealthGauge(World& world, GameObject& object, HealthGaugeComponent& gauge, int screenWidth);

	static void DrawUIObject(const World& world, const GameObject& object, int screenWidth, ID3D11ShaderResourceView* numberTexture);
	static void DrawHealthGauge(const World& world, const GameObject& object, const HealthGaugeComponent& gauge);
	static void DrawTimer(const BattleTimerComponent& timer, int screenWidth, ID3D11ShaderResourceView* numberTexture);
};
