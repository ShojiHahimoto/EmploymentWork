#pragma once

#include "Component/Component.h"
#include <SimpleMath.h>

struct VelocityComponent : public Component
{
	// 固定 60fps 前提なので、速度は 1 フレームあたりの移動量として扱う。
	DirectX::SimpleMath::Vector3 velocity = DirectX::SimpleMath::Vector3::Zero;
};
