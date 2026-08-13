#pragma once

#include "Component/Component.h"

struct BattleCameraFollowComponent : public Component
{
	// X 方向の現在速度。BattleCameraSystem が毎フレーム加速/減速させる。
	float velocityX = 0.0f;

	// 目標位置との差から、追従したい速度を作る倍率。
	float followPower = 0.12f;

	// 1 フレームあたりの最大追従速度。
	float maxSpeed = 0.55f;

	// 目標速度へ近づく時の 1 フレームあたりの加速度。
	float acceleration = 0.045f;

	// 目標へ近づいて止まる時や、向きが反転する時の 1 フレームあたりの減速度。
	float deceleration = 0.075f;

	// この距離以下まで近づいたら目標位置へ丸める。
	float snapDistance = 0.01f;

	// この速度以下まで落ちたら停止扱いにする。
	float stopSpeed = 0.005f;
};
