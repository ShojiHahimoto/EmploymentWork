#pragma once

#include "Component/Component.h"

enum class PlayerActionState
{
	Idle,
	Walk,
	VerticalJump,
	FrontJump,
	BackJump,
	Fall,
	GroundAttack,
	AirAttack,
	Hitstun,
};

struct StateComponent : public Component
{
	// StateUpdateSystem が確定した、今フレームの最終行動。
	PlayerActionState currentActionState = PlayerActionState::Idle;

	// currentActionState に入ってからの経過フレーム。
	// StateUpdateSystem が State 遷移と合わせて更新する。
	int actionFrame = 0;

	// 接地、被弾、キャンセルなどの判定材料。
	// 今後 GroundSystem / HitResolveSystem / AttackSystem が更新する想定。
	bool isGrounded = true;
	bool hitstunRequested = false;
	bool cancelEnabled = false;
};
