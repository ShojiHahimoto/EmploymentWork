#include "System/InputHistorySystem.h"

#include "Component/InputHistoryComponent.h"
#include "Input/InputSystem.h"
#include "World/World.h"

void InputHistorySystem::Update(World& world)
{
	for (GameObject& object : world.GetGameObjects())
	{
		if (object.tag != GameObjectTag::Player)
		{
			continue;
		}

		UpdateInputHistory(world, object.id);
	}
}

void InputHistorySystem::UpdateInputHistory(World& world, GameObjectId objectId)
{
	InputHistoryComponent* inputHistory = world.GetComponent<InputHistoryComponent>(objectId);
	if (!inputHistory)
	{
		return;
	}

	// 現段階では 1P の入力だけを保存する。
	// 将来は PlayerIndexComponent などで GameObject ごとの playerIndex を分ける。
	inputHistory->latestFrameIndex = 0;
	inputHistory->frames[inputHistory->latestFrameIndex] = Input::InputSystem::GetPlayerInputState(0);
}
