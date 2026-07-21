#pragma once

#include "Core/GameObject.h"

class World;
struct InputHistoryComponent;

class InputHistorySystem
{
public:
	// InputSystem で確定済みの入力を、対象 GameObject の InputHistoryComponent に保存する。
	static void Update(World& world);

private:
	static void UpdateInputHistory(World& world, GameObjectId objectId);
};
