#include "System/PlayerInvincibilitySystem.h"

#include "Component/StateComponent.h"
#include "Core/GameObject.h"
#include "Core/World.h"

/// <summary>Player の現在状態から、攻撃を受けないかどうかを StateComponent に反映する。</summary>
/// <param name="world">Player GameObject と StateComponent を保持する World。</param>
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

/// <summary>ActionState と接地状態だけで決まる無敵かどうかを判定する。</summary>
/// <param name="state">判定対象の StateComponent。</param>
/// <returns>状態由来で攻撃を受けないなら true。</returns>
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
