#pragma once

#include "Customize/MotionPose.h"
#include "Data/MotionSkeletonDefinition.h"
#include <array>

// 編集用の制御点。間にあるFBX補助ノードは固定のまま実FK階層に含める。
struct MotionIkChain
{
    std::array<MotionBodyPart, 3> parts;
    bool translationLocked = false;     //trueなら位置移動させない
};

struct MotionIkResult
{
    std::array<int, 3> boneIndices{};
    std::array<DirectX::SimpleMath::Quaternion, 3> localRotations{};
    DirectX::SimpleMath::Vector3 reachedPosition;
    float positionError = 0.0f;
    bool atReachLimit = false;
    bool atJointLimit = false;
    bool translationLocked = false;
    bool usedAlternativePose = false; // 制限回避で曲げ面/長軸回転の配分を変更した結果。
};

namespace MotionIk
{
    DirectX::SimpleMath::Vector3 MeasureLimitViolation(MotionBodyPart part,
        const DirectX::SimpleMath::Quaternion& rotation, const DirectX::SimpleMath::Quaternion& bind,
        const DirectX::SimpleMath::Quaternion& reference);
    const MotionIkChain* FindChain(MotionBodyPart target);

    bool Solve(const ModelResource& model, const SkeletonPose& basePose,
        const DirectX::SimpleMath::Matrix& objectWorld, const MotionIkChain& chain,
        const DirectX::SimpleMath::Vector3& target, MotionIkResult& result);
}
