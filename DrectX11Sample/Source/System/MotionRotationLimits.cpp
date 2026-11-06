#include "System/MotionRotationLimits.h"
#include <algorithm>
#include <cmath>

using namespace DirectX::SimpleMath;

namespace
{
    /// <summary>長軸Yのtwistと、残りのswingの回転ベクトルを度数で返す。</summary>
    /// <param name="rotation">ローカル回転。</param><param name="bind">基準姿勢。</param>
    /// <returns>X/Z=swing、Y=twist。Euler角ではない。</returns>
    Vector3 Coordinates(const Quaternion& rotation, const Quaternion& bind)
    {
        const Matrix relative = Matrix::CreateFromQuaternion(rotation) * Matrix::CreateFromQuaternion(bind).Transpose();
        Quaternion q = Quaternion::CreateFromRotationMatrix(relative); q.Normalize();
        if (q.w < 0) q = -q;
        const float length = std::hypot(q.y, q.w);
        if (length < 1e-7f) return Vector3(180, 0, 180);
        const float twist = 2 * std::atan2(q.y, q.w);
        // 行ベクトル規約: relative = twist * swing。Y軸twistを先に除く。
        Quaternion swing = Quaternion::CreateFromRotationMatrix(Matrix::CreateRotationY(-twist) * relative);
        swing.Normalize();
        if (swing.w < 0) swing = -swing;
        const float sine = std::hypot(swing.x, swing.z);
        const float scale = sine > 1e-7f ? 2 * std::atan2(sine, swing.w) / sine : 2;
        return Vector3(DirectX::XMConvertToDegrees(swing.x * scale), DirectX::XMConvertToDegrees(twist),
            DirectX::XMConvertToDegrees(swing.z * scale));
    }

    /// <summary>連続回転経路で最初の境界を探す。制限外の開始姿勢は改善だけを許可。</summary>
    /// <param name="part">部位。</param><param name="start">開始回転。</param>
    /// <param name="bind">基準回転。</param><param name="sample">0～1から回転を得る関数。</param>
    /// <returns>到達できる経路率。</returns>
    template<class Sample> float Trace(MotionBodyPart part, const Quaternion& start, const Quaternion& bind, Sample sample)
    {
        auto allowance = MotionRotationLimits::Measure(part, start, bind, start);
        float previous = 0;
        for (int step = 1; step <= 180; ++step)
        {
            const float t = float(step) / 180;
            auto value = MotionRotationLimits::Measure(part, sample(t), bind, start);
            if (!MotionRotationLimits::Permits(value, allowance))
            {
                float low = previous, high = t;
                for (int i = 0; i < 18; ++i)
                {
                    const float middle = (low + high) * 0.5f;
                    if (MotionRotationLimits::Permits(MotionRotationLimits::Measure(part, sample(middle), bind, start), allowance)) low = middle;
                    else high = middle;
                }
                return low;
            }
            for (size_t i = 0; i < value.size(); ++i) allowance[i] = std::min(allowance[i], value[i]);
            previous = t;
        }
        return 1;
    }
}

MotionRotationLimits::Violation MotionRotationLimits::Measure(MotionBodyPart part, const Quaternion& rotation,
    const Quaternion& bind, const Quaternion& reference)
{
    const auto limit = MotionSkeleton::GetPoseRotationLimit(part);
    if (!limit.enabled) return {};
    Vector3 c = Coordinates(rotation, bind);
    const float referenceTwist = Coordinates(reference, bind).y;
    c.y = referenceTwist + std::remainder(c.y - referenceTwist, 360.0f);
    Violation result{};
    for (int axis = 0; axis < 3; ++axis)
    {
        result[axis * 2] = std::max(0.0f, (&limit.minimum.x)[axis] - (&c.x)[axis]);
        result[axis * 2 + 1] = std::max(0.0f, (&c.x)[axis] - (&limit.maximum.x)[axis]);
    }
    result[6] = std::max(0.0f, std::hypot(c.x, c.z) - limit.maxSwingDegrees);
    return result;
}

bool MotionRotationLimits::Permits(const Violation& value, const Violation& allowance)
{
    for (size_t i = 0; i < value.size(); ++i)
        if (!std::isfinite(value[i]) || value[i] > allowance[i] + 0.002f) return false;
    return true;
}

Quaternion MotionRotationLimits::Constrain(MotionBodyPart part, const Quaternion& start,
    const Quaternion& target, const Quaternion& bind)
{
    const auto sample = [&](float t) { Quaternion q = Quaternion::Slerp(start, target, t); q.Normalize(); return q; };
    Quaternion result = sample(Trace(part, start, bind, sample));
    return result;
}

Vector2 MotionRotationLimits::AxisRange(MotionBodyPart part, const Quaternion& start, const Vector3& axis, const Quaternion& bind)
{
    Vector2 result;
    for (int side = 0; side < 2; ++side)
    {
        const float angle = (side == 0 ? -1.0f : 1.0f) * DirectX::XM_2PI;
        const auto sample = [&](float t) { return Quaternion::CreateFromAxisAngle(axis, angle * t) * start; };
        (&result.x)[side] = angle * Trace(part, start, bind, sample);
    }
    return result;
}
