#pragma once

#include "System/MotionPose.h"
#include "Data/MotionSkeletonDefinition.h"
#include <array>

// 編集用の制御点。間にあるFBX補助ノードは固定のまま実FK階層に含める。
struct MotionIkChain
{
    std::array<MotionBodyPart, 3> parts;
};

struct MotionIkResult
{
    std::array<int, 3> boneIndices{};
    std::array<DirectX::SimpleMath::Quaternion, 3> localRotations{};
    DirectX::SimpleMath::Vector3 reachedPosition;
    float positionError = 0.0f;
    bool atReachLimit = false;
};

namespace MotionIk
{
    /// <summary>選択部位から、回転を保存できる親2点と対象点を決める。</summary>
    /// <param name="target">動かす部位。</param>
    /// <returns>対応チェーン。非対応部位はnullptr。</returns>
    const MotionIkChain* FindChain(MotionBodyPart target);

    /// <summary>開始姿勢から指定位置へ直線上で移動し、実FKで一致を検証した回転だけを返す。</summary>
    /// <param name="model">補助ノードも含む実骨階層。</param>
    /// <param name="basePose">ドラッグ開始姿勢。呼び出しで変更しない。</param>
    /// <param name="objectWorld">モデル本体のワールド変換。</param>
    /// <param name="chain">固定根元、中間、移動対象の編集部位。</param>
    /// <param name="target">要求したワールド位置。到達結果で上書きしない。</param>
    /// <param name="result">成功時の回転、到達位置と誤差。</param>
    /// <returns>実階層・変換が有効で、回転復元結果が位置解に一致すればtrue。</returns>
    bool Solve(const ModelResource& model, const SkeletonPose& basePose,
        const DirectX::SimpleMath::Matrix& objectWorld, const MotionIkChain& chain,
        const DirectX::SimpleMath::Vector3& target, MotionIkResult& result);
}
