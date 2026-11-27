#pragma once

#include <array>
#include <d3d11.h>

struct BattleTimerComponent;
struct GameObject;
struct HealthGaugeComponent;
struct TransformComponent;
class World;
class Renderer;

class BattleHUDSystem
{
public:
	static void Update(World& world, int screenWidth, int screenHeight);

	static constexpr std::array TexturePaths{
		"assets/texture/UI/Parts/Battle/HP_Frame.png",
		"assets/texture/UI/Parts/Battle/HP_Player1.png",
		"assets/texture/UI/Parts/Battle/HP_Player2.png",
		"assets/texture/UI/Parts/Battle/HP_Damage.png",
		"assets/texture/UI/Parts/Battle/PlayerPlate_P1.png",
		"assets/texture/UI/Parts/Battle/Icon_Player1.png",
		"assets/texture/UI/Parts/Battle/Icon_Player2.png",
		"assets/texture/UI/Parts/Battle/Round_Empty.png",
		"assets/texture/UI/Parts/Battle/Timer_Digits.png",
		"assets/texture/UI/Parts/SlotSelect/badge_a.png",
		"assets/texture/UI/Parts/SlotSelect/badge_b.png",
		"assets/texture/UI/Parts/SlotSelect/badge_x.png",
		"assets/texture/UI/Parts/SlotSelect/badge_y.png",
		"assets/texture/UI/Parts/Battle/PlayerPlate_P2.png"
	};
	using Textures = std::array<ID3D11ShaderResourceView*, TexturePaths.size()>;
	static void Draw(Renderer& renderer, const World& world, int screenWidth, int screenHeight,
		ID3D11ShaderResourceView* numberTexture, const Textures& textures);

private:
	static void UpdateUIObject(World& world, GameObject& object, int screenWidth);
	static void UpdateHealthGauge(World& world, GameObject& object, HealthGaugeComponent& gauge, int screenWidth);

};
