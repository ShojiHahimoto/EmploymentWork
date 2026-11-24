#pragma once

struct StateComponent;
class World;

class PlayerInvincibilitySystem
{
public:
	static void Update(World& world);

	static bool IsInvincibleByState(const StateComponent& state);
};
