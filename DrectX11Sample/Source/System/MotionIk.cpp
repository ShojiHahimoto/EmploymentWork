#include "System/MotionIk.h"
#include "System/MotionRotationLimits.h"
#include <algorithm>
#include <cmath>

using namespace DirectX::SimpleMath;

namespace
{
    using Part = MotionBodyPart;
    const MotionIkChain Chains[] = {
        {{Part::RShoulder, Part::RElbow, Part::RHand}},
        {{Part::LShoulder, Part::LElbow, Part::LHand}},
        {{Part::RHipjoint, Part::RKnees, Part::RFeet}},
        {{Part::LHipjoint, Part::LKnees, Part::LFeet}},
        // 肘膝は上腕/大腿の根元固定。肩股関節は反対側/下半身の固定条件を持つ。
        // 現15部位の回転自由度では有限の直線をたどれないため、入力を止める。
        // 祖先を勝手に動かしたり球面に投影して「平行移動」と表示しない。
        {{Part::RShoulder, Part::RElbow, Part::RElbow}, true},
        {{Part::LShoulder, Part::LElbow, Part::LElbow}, true},
        {{Part::RHipjoint, Part::RKnees, Part::RKnees}, true},
        {{Part::LHipjoint, Part::LKnees, Part::LKnees}, true},
        {{Part::Spine, Part::RShoulder, Part::RShoulder}, true},
        {{Part::Spine, Part::LShoulder, Part::LShoulder}, true},
        {{Part::Waist, Part::RHipjoint, Part::RHipjoint}, true},
        {{Part::Waist, Part::LHipjoint, Part::LHipjoint}, true}
    };

    /// <summary>小さい区間や境界の計算で桁落ちを抑える内積。</summary>
    /// <param name="a">第1ベクトル。</param><param name="b">第2ベクトル。</param>
    /// <returns>倍精度内積。</returns>
    double Dot(const Vector3& a, const Vector3& b)
    {
        return double(a.x)*b.x + double(a.y)*b.y + double(a.z)*b.z;
    }

    /// <summary>有限値のみを計算に通す。</summary>
    /// <param name="v">検査値。</param><returns>有限ならtrue。</returns>
    bool Finite(const Vector3& v)
    {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    }

    /// <summary>途中の補助ノードもたどり、実際の祖先関係を検証する。</summary>
    /// <param name="bones">親を先に登録した実骨階層。</param>
    /// <param name="ancestor">祖先候補。</param><param name="child">子孫候補。</param>
    /// <returns>異なるノードで祖先関係があればtrue。</returns>
    bool IsAncestor(const std::vector<ModelBone>& bones, int ancestor, int child)
    {
        for (size_t count = 0; count < bones.size(); ++count)
        {
            const int parent = bones[child].parentIndex;
            if (parent < 0 || parent >= child) return false;
            if (parent == ancestor) return true;
            child = parent;
        }
        return false;
    }

    /// <summary>非一様拡縮・せん断は回転だけでは位置解を再現できないため拒否する。</summary>
    /// <param name="m">対象ワールド行列。</param><returns>正の一様拡縮と回転ならtrue。</returns>
    bool IsRigidScaled(const Matrix& m)
    {
        Vector3 x(m._11,m._12,m._13), y(m._21,m._22,m._23), z(m._31,m._32,m._33);
        const float s = x.LengthSquared();
        return Finite(x) && Finite(y) && Finite(z) && s > 1e-12f && m.Determinant() > 0
            && std::abs(y.LengthSquared()-s) < s*1e-3f && std::abs(z.LengthSquared()-s) < s*1e-3f
            && std::abs(x.Dot(y)) < s*1e-3f && std::abs(x.Dot(z)) < s*1e-3f && std::abs(y.Dot(z)) < s*1e-3f;
    }

    /// <summary>方向を合わせる最短回転。反平行時の軸は開始姿勢の曲げ基準を使う。</summary>
    /// <param name="from">元の方向。</param><param name="to">新方向。</param>
    /// <param name="pole">反転時の基準方向。</param><returns>ワールド回転差分。</returns>
    Matrix DirectionRotation(Vector3 from, Vector3 to, Vector3 pole)
    {
        from.Normalize(); to.Normalize();
        const float dot = std::clamp(from.Dot(to), -1.0f, 1.0f);
        Vector3 cross = from.Cross(to);
        const float sine = cross.Length();
        if (sine > 1e-7f)
            return Matrix::CreateFromAxisAngle(cross / sine, std::atan2(sine, dot));
        if (dot >= 0) return Matrix::Identity;
        pole -= from * pole.Dot(from);
        if (pole.LengthSquared() < 1e-8f)
        {
            pole = std::abs(from.x) < 0.8f ? Vector3::UnitX : Vector3::UnitY;
            pole -= from * pole.Dot(from);
        }
        pole.Normalize();
        return Matrix::CreateFromAxisAngle(pole, DirectX::XM_PI);
    }

    /// <summary>開始点から要求点への線分が到達可能な球殻を初めて出る位置で止める。</summary>
    /// <param name="start">根元を原点とした開始点。</param><param name="delta">要求移動。</param>
    /// <param name="minimum">最小到達半径。</param><param name="maximum">最大到達半径。</param>
    /// <returns>移動率0～1。球面への投影は行わない。</returns>
    double ReachFraction(const Vector3& start, const Vector3& delta, double minimum, double maximum)
    {
        const double a = Dot(delta, delta);
        if (a < 1e-20) return 1;
        const double b = Dot(start, delta), distance2 = Dot(start, start);
        // 丸め誤差で開始点自身を到達不能扱いしない。
        maximum = std::max(maximum, std::sqrt(distance2));
        minimum = std::min(minimum, std::sqrt(distance2));
        double fraction = 1;
        double disc = std::max(0.0, b*b - a*(distance2-maximum*maximum));
        fraction = std::min(fraction, (-b + std::sqrt(disc))/a);
        // 内側の球へ入る場合も最初の境界で止める。反対側へ飛び越えさせない。
        disc = b*b - a*(distance2-minimum*minimum);
        if (disc > 0 && b < 0)
        {
            const double enter = (-b-std::sqrt(disc))/a;
            if (enter >= 0) fraction = std::min(fraction, enter);
        }
        return std::clamp(fraction, 0.0, 1.0);
    }

    /// <summary>ワールド差分を右から適用し、実親階層を通してローカル回転へ戻す。</summary>
    /// <param name="pose">更新する作業姿勢。</param><param name="model">実骨階層。</param>
    /// <param name="object">本体変換。</param><param name="bone">回すノード。</param>
    /// <param name="desired">目標ワールド行列。</param><returns>回転復元成功時true。</returns>
    bool SetWorldRotation(SkeletonPose& pose, const ModelResource& model, const Matrix& object,
        int bone, const Matrix& desired)
    {
        BonePose local;
        if (!MotionPose::WorldToLocalPose(pose, model, bone, object, desired, local)) return false;
        // ローカル位置と長さは一切変更しない。補助ノードも元のまま残す。
        pose.bonePoses[bone].localRotation = local.localRotation;
        MotionPose::UpdateSkinningMatrices(pose, model);
        return true;
    }
}

const MotionIkChain* MotionIk::FindChain(MotionBodyPart target)
{
    for (const auto& chain : Chains) if (chain.parts[2] == target) return &chain;
    return nullptr;
}

/// <summary>可動域探索用の幾何解。外部は必ず制限付きSolveを経由する。</summary>
/// <param name="model">モデル。</param><param name="basePose">開始姿勢。</param>
/// <param name="objectWorld">本体変換。</param><param name="chain">対象階層。</param>
/// <param name="target">目標。</param><param name="result">検証済みFK解。</param>
/// <param name="swivel">手足への直線を軸にした肘膝の回り込み角。</param>
/// <param name="alignHinge">曲げ面に合わせる上腕/大腿の長軸補正率0～1。</param>
/// <returns>有効な幾何解ならtrue。</returns>
static bool SolveUnconstrained(const ModelResource& model, const SkeletonPose& basePose,
    const Matrix& objectWorld, const MotionIkChain& chain, const Vector3& target, MotionIkResult& result,
    float swivel = 0.0f, float alignHinge = 0.0f)
{
    if (!basePose.initialized || !Finite(target) || basePose.bonePoses.size() != model.GetBones().size()) return false;
    MotionIkResult candidate;
    std::array<Matrix, 3> worlds;
    std::array<Vector3, 3> points;
    for (int i = 0; i < 3; ++i)
    {
        candidate.boneIndices[i] = MotionSkeleton::FindModelBoneIndex(model,
            MotionSkeleton::GetBodyPartName(static_cast<int>(chain.parts[i])));
        if (!MotionPose::GetBoneWorldMatrix(basePose, candidate.boneIndices[i], objectWorld, worlds[i])
            || !IsRigidScaled(worlds[i])) return false;
        points[i] = worlds[i].Translation();
        candidate.localRotations[i] = basePose.bonePoses[candidate.boneIndices[i]].localRotation;
    }
    if (chain.translationLocked)
    {
        candidate.reachedPosition = points[2];
        candidate.translationLocked = true;
        candidate.atReachLimit = Vector3::DistanceSquared(points[2], target) > 1e-14f;
        result = candidate;
        return true;
    }
    const auto& bones = model.GetBones();
    if (!IsAncestor(bones,candidate.boneIndices[0],candidate.boneIndices[1])
        || !IsAncestor(bones,candidate.boneIndices[1],candidate.boneIndices[2])) return false;

    const Vector3 start = points[2]-points[0], upper = points[1]-points[0];
    const double a = std::sqrt(Dot(upper,upper)), b = Vector3::Distance(points[1],points[2]);
    if (a < 1e-6 || b < 1e-6 || start.LengthSquared() < 1e-12f) return false;
    const Vector3 move = target-points[2];
    const double fraction = ReachFraction(start,move,std::max(std::abs(a-b),(a+b)*1e-6),a+b);
    const Vector3 end = points[2] + move * static_cast<float>(fraction);
    candidate.atReachLimit = fraction < 1.0-1e-6;
    candidate.reachedPosition = points[2];
    // 限界で停止する操作やクリックだけで再分解して姿勢を変えない。
    if (Vector3::DistanceSquared(end,points[2]) < 1e-14f) { result = candidate; return true; }

    Vector3 oldDirection = start; oldDirection.Normalize();
    Vector3 direction = end-points[0];
    const double distance = std::sqrt(Dot(direction,direction));
    if (distance < 1e-8) return false;
    direction /= static_cast<float>(distance);
    Vector3 pole = upper-oldDirection*upper.Dot(oldDirection);
    // 伸び切りでは曲げ平面が未定義。開始関節の座標軸から一定の基準を選ぶ。
    // 非ゼロの小さなFBX誤差を増幅させないため、相対閾値を設ける。
    if (pole.LengthSquared() < a*a*1e-6)
    {
        // 伸び切りで選ぶ曲げ面は、肘/膝の許可された曲げ方向から作る。
        // 根元のZ軸を使うだけでは、左右や脚で逆曲がりの側を選んでしまう。
        const auto limit = MotionSkeleton::GetPoseRotationLimit(chain.parts[1]);
        const Vector3 hinge = Vector3::TransformNormal(Vector3::UnitX, worlds[1]);
        const float bendSign = limit.maximum.x > 0.0f ? 1.0f : -1.0f;
        pole = -bendSign * hinge.Cross(oldDirection);
        pole -= oldDirection*pole.Dot(oldDirection);
        if (pole.LengthSquared() < 1e-10f)
        {
            pole = Vector3::TransformNormal(Vector3::UnitY, worlds[0]);
            pole -= oldDirection*pole.Dot(oldDirection);
        }
    }
    pole.Normalize();
    pole = Vector3::TransformNormal(pole, DirectionRotation(oldDirection,direction,pole));
    pole -= direction*pole.Dot(direction);
    pole.Normalize();
    const double along = (a*a-b*b+distance*distance)/(2*distance);
    // 手足位置を変えずに肘膝が回り込める自由度。制限探索から指定する。
    pole = Vector3::TransformNormal(pole, Matrix::CreateFromAxisAngle(direction, swivel));
    const double height = std::sqrt(std::max(0.0,a*a-along*along));
    const Vector3 middle = points[0] + direction*static_cast<float>(along) + pole*static_cast<float>(height);

    SkeletonPose solved = basePose;
    // 行ベクトルのDirectX規約では既存ワールド行列の右側にワールド差分を掛ける。
    // 以前のdelta * currentQuaternionは順序が逆で、回転済みモデルの方向を壊していた。
    Matrix root = worlds[0]; root.Translation(Vector3::Zero);
    root *= DirectionRotation(upper,middle-points[0],pole);
    root.Translation(points[0]);
    if (!SetWorldRotation(solved,model,objectWorld,candidate.boneIndices[0],root)) return false;
    Matrix midWorld, endWorld;
    MotionPose::GetBoneWorldMatrix(solved,candidate.boneIndices[1],objectWorld,midWorld);
    MotionPose::GetBoneWorldMatrix(solved,candidate.boneIndices[2],objectWorld,endWorld);
    if (alignHinge)
    {
        // 上腕/大腿の長軸回転は肘膝の位置を変えない。曲げ軸を目標の曲げ面へ
        // 合わせることで、単に肘を横曲げさせて可動域に当てる解を避ける。
        Vector3 upperDirection = middle - points[0]; upperDirection.Normalize();
        Vector3 normal = upperDirection.Cross(end - middle);
        const float bendSign = MotionSkeleton::GetPoseRotationLimit(chain.parts[1]).maximum.x > 0 ? 1.0f : -1.0f;
        normal *= bendSign;
        Vector3 hinge = Vector3::TransformNormal(Vector3::UnitX, midWorld);
        hinge -= upperDirection * hinge.Dot(upperDirection);
        if (normal.LengthSquared() > 1e-10f && hinge.LengthSquared() > 1e-10f)
        {
            normal.Normalize(); hinge.Normalize();
            const float roll = alignHinge * std::atan2(upperDirection.Dot(hinge.Cross(normal)), hinge.Dot(normal));
            root.Translation(Vector3::Zero);
            root *= Matrix::CreateFromAxisAngle(upperDirection, roll);
            root.Translation(points[0]);
            if (!SetWorldRotation(solved, model, objectWorld, candidate.boneIndices[0], root)) return false;
            MotionPose::GetBoneWorldMatrix(solved,candidate.boneIndices[1],objectWorld,midWorld);
            MotionPose::GetBoneWorldMatrix(solved,candidate.boneIndices[2],objectWorld,endWorld);
        }
    }
    const Vector3 actualMiddle = midWorld.Translation();
    Matrix rotatedMiddle = midWorld; rotatedMiddle.Translation(Vector3::Zero);
    rotatedMiddle *= DirectionRotation(endWorld.Translation()-actualMiddle,end-actualMiddle,pole);
    rotatedMiddle.Translation(actualMiddle);
    if (!SetWorldRotation(solved,model,objectWorld,candidate.boneIndices[1],rotatedMiddle)) return false;

    // 手足自身のローカル回転は開始時のまま。親に追従するワールド姿勢を許可し、
    // 向き固定のための手首/足首補正でIK全体を止めない。
    MotionPose::GetBoneWorldMatrix(solved,candidate.boneIndices[2],objectWorld,endWorld);
    candidate.reachedPosition = endWorld.Translation();
    candidate.positionError = Vector3::Distance(candidate.reachedPosition,end);
    const float tolerance = static_cast<float>((a+b)*1e-4+1e-5);
    if (!Finite(candidate.reachedPosition) || candidate.positionError > tolerance
        || Vector3::Distance(actualMiddle,middle) > tolerance) return false;
    for (int i = 0; i < 3; ++i) candidate.localRotations[i] = solved.bonePoses[candidate.boneIndices[i]].localRotation;
    candidate.usedAlternativePose = std::abs(swivel) > 1e-5f || alignHinge > 1e-5f;
    result = candidate;
    return true;
}

Vector3 MotionIk::MeasureLimitViolation(Part part, const Quaternion& rotation,
    const Quaternion& bind, const Quaternion& reference)
{
    const auto v = MotionRotationLimits::Measure(part, rotation, bind, reference);
    return Vector3(std::max(v[0], v[1]), std::max(v[2], v[3]), std::max({v[4], v[5], v[6]}));
}

bool MotionIk::Solve(const ModelResource& model, const SkeletonPose& basePose,
    const Matrix& objectWorld, const MotionIkChain& chain, const Vector3& target, MotionIkResult& result)
{
    MotionIkResult requested;
    if (!SolveUnconstrained(model, basePose, objectWorld, chain, target, requested)) return false;
    if (chain.translationLocked) { result = requested; return true; }
    Matrix startWorld;
    MotionPose::GetBoneWorldMatrix(basePose, requested.boneIndices[2], objectWorld, startWorld);
    const Vector3 start = startWorld.Translation();
    const Vector3 delta = requested.reachedPosition - start;
    MotionIkResult accepted;
    if (!SolveUnconstrained(model, basePose, objectWorld, chain, start, accepted)) return false;
    if (delta.LengthSquared() < 1e-14f) { result = requested; return true; }

    std::array<MotionRotationLimits::Violation, 2> allowance;
    for (int i = 0; i < 2; ++i)
    {
        const int bone = accepted.boneIndices[i];
        allowance[i] = MotionRotationLimits::Measure(chain.parts[i], accepted.localRotations[i],
            model.GetBones()[bone].bindLocalRotation, accepted.localRotations[i]);
    }
    // 手足のローカル回転は不変なので、制限探索の対象は根元と中間だけ。
    const auto permits = [&](const MotionIkResult& pose)
    {
        for (int i = 0; i < 2; ++i)
        {
            const int bone = pose.boneIndices[i];
            if (!MotionRotationLimits::Permits(MotionRotationLimits::Measure(chain.parts[i], pose.localRotations[i],
                model.GetBones()[bone].bindLocalRotation, basePose.bonePoses[bone].localRotation), allowance[i])) return false;
        }
        return true;
    };
    float swivel = 0, alignment = 0;
    // 固定した一つの曲げ方だけで到達不能と決めない。位置を固定したまま
    // 肘膝の回り込みと上腕/大腿の長軸回転を探索する。保存されるのは回転のみ。
    const auto solveAt = [&](float fraction, MotionIkResult& pose, float& chosenSwivel, float& chosenAlignment)
    {
        const Vector3 end = start + delta * fraction;
        if (SolveUnconstrained(model, basePose, objectWorld, chain, end, pose, swivel, alignment) && permits(pose))
        { chosenSwivel = swivel; chosenAlignment = alignment; return true; }
        bool found = false;
        float best = 1e9f;
        MotionIkResult candidate;
        for (float degrees : {0.f, -5.f, 5.f, -15.f, 15.f, -45.f, 45.f, -90.f, 90.f, 180.f})
        {
            for (float weight : {0.f, 0.25f, 0.5f, 0.75f, 1.f})
            {
                const float angle = swivel + DirectX::XMConvertToRadians(degrees);
                if (!SolveUnconstrained(model, basePose, objectWorld, chain, end, candidate, angle, weight) || !permits(candidate)) continue;
                float change = 0;
                for (int i = 0; i < 2; ++i)
                    change += 1 - std::abs(candidate.localRotations[i].Dot(accepted.localRotations[i]));
                if (change < best)
                {
                    best = change; pose = candidate; chosenSwivel = angle; chosenAlignment = weight; found = true;
                }
            }
            // 小さい補正で解けるなら、不要な反対側の解を探して姿勢を切り替えない。
            if (found && best < 0.002f) break;
        }
        if (!found) return false;
        // 粗い探索の5度単位をそのまま適用せず、元の回転配分から最初の有効点まで細分化。
        float low = 0, high = 1;
        const float endSwivel = chosenSwivel, endAlignment = chosenAlignment;
        for (int i = 0; i < 12; ++i)
        {
            const float t = (low + high) * 0.5f;
            const float angle = swivel + (endSwivel - swivel) * t;
            const float weight = alignment + (endAlignment - alignment) * t;
            if (SolveUnconstrained(model, basePose, objectWorld, chain, end, candidate, angle, weight) && permits(candidate))
            { high = t; pose = candidate; chosenSwivel = angle; chosenAlignment = weight; }
            else low = t;
        }
        // 別の解への大きな飛び移りは採用しない。移動量を減らして連続にたどれる範囲で止める。
        for (int i = 0; i < 2; ++i)
            if (std::abs(pose.localRotations[i].Dot(accepted.localRotations[i])) < 0.98f) return false;
        return true;
    };
    constexpr int Steps = 24;
    float previous = 0;
    for (int step = 1; step <= Steps; ++step)
    {
        const float fraction = float(step) / Steps;
        MotionIkResult candidate;
        float angle = swivel, weight = alignment;
        if (!solveAt(fraction, candidate, angle, weight))
        {
            float low = previous, high = fraction;
            MotionIkResult boundary = accepted;
            for (int iteration = 0; iteration < 14; ++iteration)
            {
                const float middle = (low + high) * 0.5f;
                if (solveAt(middle, candidate, angle, weight))
                { low = middle; boundary = candidate; }
                else high = middle;
            }
            boundary.atJointLimit = true;
            result = boundary;
            return true;
        }
        accepted = candidate; swivel = angle; alignment = weight;
        for (int i = 0; i < 2; ++i)
        {
            const int bone = accepted.boneIndices[i];
            const auto v = MotionRotationLimits::Measure(chain.parts[i], accepted.localRotations[i],
                model.GetBones()[bone].bindLocalRotation, basePose.bonePoses[bone].localRotation);
            for (size_t j = 0; j < v.size(); ++j) allowance[i][j] = std::min(allowance[i][j], v[j]);
        }
        previous = fraction;
    }
    accepted.atReachLimit = requested.atReachLimit;
    result = accepted;
    return true;
}
