#pragma once
#include "Data/MotionSkeletonDefinition.h"
#include <array>

namespace MotionRotationLimits
{
    // 各軸の下限/上限とswing円錐の逸脱。逆側への飛び移りを許さないため別々に保持。
    using Violation = std::array<float, 7>;

    Violation Measure(MotionBodyPart part, const DirectX::SimpleMath::Quaternion& rotation,
        const DirectX::SimpleMath::Quaternion& bind, const DirectX::SimpleMath::Quaternion& reference);

    bool Permits(const Violation& value, const Violation& allowance);

    DirectX::SimpleMath::Quaternion Constrain(MotionBodyPart part,
        const DirectX::SimpleMath::Quaternion& start, const DirectX::SimpleMath::Quaternion& target,
        const DirectX::SimpleMath::Quaternion& bind);

    DirectX::SimpleMath::Vector2 AxisRange(MotionBodyPart part,
        const DirectX::SimpleMath::Quaternion& start, const DirectX::SimpleMath::Vector3& axis,
        const DirectX::SimpleMath::Quaternion& bind);
}
