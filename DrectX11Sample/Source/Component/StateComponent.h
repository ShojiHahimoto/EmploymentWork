#pragma once

#include "Component/Component.h"

enum class ObjectState
{
	Idle,
	Walk,
	Jump,
	Fall,
};

struct StateComponent : public Component
{
	// 現在の状態と、その状態に入ってからの経過フレーム。
	// 状態遷移や frame 更新は StateComponent ではなく System が担当する。
	ObjectState currentState = ObjectState::Idle;
	int stateFrame = 0;
};
