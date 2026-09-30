#pragma once
#include "Data/MotionSkeletonDefinition.h"
#include <array>

namespace MotionRotationLimits
{
    // 各軸の下限/上限とswing円錐の逸脱。逆側への飛び移りを許さないため別々に保持。
    using Violation = std::array<float, 7>;

    /// <summary>Quaternionのまま曲げと長軸ひねりを測り、可動域からの逸脱を返す。</summary>
    /// <param name="part">部位。</param><param name="rotation">ローカル回転。</param>
    /// <param name="bind">実FBXの基準回転。</param><param name="reference">ひねりの連続性基準。</param>
    /// <returns>度数の逸脱量。0なら範囲内。</returns>
    Violation Measure(MotionBodyPart part, const DirectX::SimpleMath::Quaternion& rotation,
        const DirectX::SimpleMath::Quaternion& bind, const DirectX::SimpleMath::Quaternion& reference);

    /// <summary>既存の制限外姿勢も壊さず、逸脱が増えないか判定する。</summary>
    /// <param name="value">候補の逸脱。</param><param name="allowance">開始時の許容逸脱。</param>
    /// <returns>誤差許容量内ならtrue。</returns>
    bool Permits(const Violation& value, const Violation& allowance);

    /// <summary>数値入力による目標回転への最短経路上で、最初の可動域境界に止める。</summary>
    /// <param name="part">部位。</param><param name="start">編集前。</param>
    /// <param name="target">入力から一度だけ変換したQuaternion。</param><param name="bind">基準回転。</param>
    /// <returns>補正済み回転。Eulerへの再分解はしない。</returns>
    DirectX::SimpleMath::Quaternion Constrain(MotionBodyPart part,
        const DirectX::SimpleMath::Quaternion& start, const DirectX::SimpleMath::Quaternion& target,
        const DirectX::SimpleMath::Quaternion& bind);

    /// <summary>ローカル1軸ギズモが連続して回せる角度範囲をQuaternionで探索する。</summary>
    /// <param name="part">部位。</param><param name="start">ドラッグ開始回転。</param>
    /// <param name="axis">操作ローカル軸。</param><param name="bind">基準回転。</param>
    /// <returns>負方向限界と正方向限界のラジアン。</returns>
    DirectX::SimpleMath::Vector2 AxisRange(MotionBodyPart part,
        const DirectX::SimpleMath::Quaternion& start, const DirectX::SimpleMath::Vector3& axis,
        const DirectX::SimpleMath::Quaternion& bind);
}
