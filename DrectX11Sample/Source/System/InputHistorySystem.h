#pragma once

#include "Component/InputHistoryComponent.h"
#include "Core/GameObject.h"
#include "Input/InputTypes.h"

class World;

class InputHistorySystem
{
public:
	// InputSystem で確定済みの入力を、格闘ゲーム用の入力履歴に変換して保存する。
	static void Update(World& world);

private:
	static void UpdateInputHistory(World& world, GameObjectId objectId);
	static InputHistoryFrame BuildHistoryFrame(const Input::PlayerInputState& inputState);
	static int ConvertMoveAxisToDirection(const DirectX::SimpleMath::Vector2& moveAxis);
	static InputButtonHistoryState CopyButtonState(const Input::InputActionState& actionState);
};
