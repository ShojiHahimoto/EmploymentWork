#include "System/PlayerInvincibilitySystem.h"

#include "Component/StateComponent.h"
#include "Core/GameObject.h"
#include "World/World.h"

void PlayerInvincibilitySystem::Update(World& world)
{
	for (GameObject& object : world.GetGameObjects())
	{
		if (object.tag != GameObjectTag::Player)
		{
			continue;
		}

		StateComponent* state = world.GetComponent<StateComponent>(object.id);
		if (!state)
		{
			continue;
		}

		// 技フレーム由来の無敵を追加する場合も、ここで isInvincible の判定に合流させる。
		state->isInvincible = IsInvincibleByState(*state);
	}
}

bool PlayerInvincibilitySystem::IsInvincibleByState(const StateComponent& state)
{
	if (state.currentActionState == PlayerActionState::Down
		|| state.currentActionState == PlayerActionState::WakeUp)
	{
		return true;
	}

	// AirHitstun が着地した直後、Down に遷移するまでの同一フレームだけ追撃を受けないようにする。
	return state.currentActionState == PlayerActionState::AirHitstun && state.isGrounded;
}
