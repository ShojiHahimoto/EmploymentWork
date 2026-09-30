#include "System/MotionPose.h"
#include "System/MotionIk.h"
#include "System/MotionRotationLimits.h"
#include "Data/MotionDataLoader.h"
#include "Data/MotionDataSaver.h"
#include "Data/MotionSkeletonDefinition.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cassert>
#include <iostream>
#include <filesystem>
#include <chrono>
#undef assert
#define assert(condition) do { if (!(condition)) { std::cerr << "CHECK FAILED line " << __LINE__ << ": " << #condition << '\n'; std::exit(1); } } while (false)

static int checks = 0;
static int alternativeSolutions = 0;

using namespace DirectX::SimpleMath;
static std::vector<ModelBone> testBones;
const std::vector<ModelBone>& ModelResource::GetBones() const { return testBones; }
int ModelResource::FindBoneIndex(const std::string& name) const
{
    for (size_t i = 0; i < testBones.size(); ++i)
        if (testBones[i].name == name) return static_cast<int>(i);
    return -1;
}
ModelMesh::~ModelMesh() = default;
ModelMaterial::~ModelMaterial() = default;

// Production loader uses the same transpose and full node hierarchy, without GPU resources here.
static void ReadNodes(const aiNode& node, int parent)
{
    const auto& m = node.mTransformation;
    ModelBone bone;
    bone.name = node.mName.C_Str();
    bone.parentIndex = parent;
    bone.bindLocalMatrix = Matrix(m.a1,m.b1,m.c1,m.d1,m.a2,m.b2,m.c2,m.d2,
        m.a3,m.b3,m.c3,m.d3,m.a4,m.b4,m.c4,m.d4);
    assert(bone.bindLocalMatrix.Decompose(bone.bindLocalScale, bone.bindLocalRotation, bone.bindLocalPosition));
    bone.bindLocalRotation.Normalize();
    const int index = static_cast<int>(testBones.size());
    testBones.push_back(bone);
    for (unsigned i = 0; i < node.mNumChildren; ++i) ReadNodes(*node.mChildren[i], index);
}

// Test actual FK after baking, not just solver coordinates. Unedited world coordinates must stay fixed.
static void CheckSolve(const ModelResource& model, const SkeletonPose& base, const Matrix& object,
    const MotionIkChain& chain, int axis, float amount)
{
    const int endIndex = MotionSkeleton::FindModelBoneIndex(model, MotionSkeleton::GetBodyPartName(static_cast<int>(chain.parts[2])));
    Matrix startWorld;
    assert(MotionPose::GetBoneWorldMatrix(base,endIndex,object,startWorld));
    Vector3 start = startWorld.Translation(), target = start;
    (&target.x)[axis] += amount;
    MotionIkResult result;
    if (!MotionIk::Solve(model,base,object,chain,target,result))
    {
        std::cerr << "Solve failure: " << MotionSkeleton::GetBodyPartName(static_cast<int>(chain.parts[2]))
            << " axis=" << axis << " amount=" << amount << '\n';
        std::exit(1);
    }
    for (int i = 0; i < 3; ++i) if (i != axis) assert(std::abs((&result.reachedPosition.x)[i]-(&start.x)[i]) < 0.002f);
    if (result.usedAlternativePose) ++alternativeSolutions;
    const float distance = (&result.reachedPosition.x)[axis]-(&start.x)[axis];
    assert(distance*amount >= -0.001f && std::abs(distance) <= std::abs(amount)+0.002f);
    if (!result.atReachLimit && !result.atJointLimit) assert(Vector3::Distance(target,result.reachedPosition) < 0.002f);
    // 可動域を越えた既存姿勢は勝手に矯正しないが、IKでさらに悪化させない。
    for (int i = 0; i < 3; ++i)
    {
        const int bone = result.boneIndices[i];
        const auto original = base.bonePoses[bone].localRotation;
        const auto before = MotionIk::MeasureLimitViolation(chain.parts[i], original, testBones[bone].bindLocalRotation, original);
        const auto after = MotionIk::MeasureLimitViolation(chain.parts[i], result.localRotations[i], testBones[bone].bindLocalRotation, original);
        for (int axisIndex = 0; axisIndex < 3; ++axisIndex)
            assert((&after.x)[axisIndex] <= (&before.x)[axisIndex] + 0.003f);
        if (chain.translationLocked) assert(std::abs(original.Dot(result.localRotations[i])) > 0.99999f);
    }

    // A saved key contains only the same three quaternion rotations. Sampling it must reproduce the solution.
    MotionData motion;
    motion.totalFrames = 1;
    for (int i = 0; i < 3; ++i)
    {
        MotionBoneTrackData track;
        track.boneName = MotionSkeleton::GetBodyPartName(static_cast<int>(chain.parts[i]));
        MotionBoneKeyframeData key;
        key.frame = 0; key.hasRotation = true; key.localRotation = result.localRotations[i];
        track.keyframes.push_back(key); motion.boneTracks.push_back(track);
    }
    SkeletonPose sampled;
    MotionPose::ApplyMotionData(sampled,motion,0,model,&base);
    MotionPose::UpdateSkinningMatrices(sampled,model);
    Matrix actual;
    assert(MotionPose::GetBoneWorldMatrix(sampled,endIndex,object,actual));
    assert(Vector3::Distance(actual.Translation(),result.reachedPosition) < 0.002f);
    for (size_t i = 0; i < base.bonePoses.size(); ++i)
    {
        assert(base.bonePoses[i].localPosition == sampled.bonePoses[i].localPosition);
        assert(base.bonePoses[i].localScale == sampled.bonePoses[i].localScale);
        const int parent = testBones[i].parentIndex;
        if (parent >= 0)
            assert(std::abs(Vector3::Distance(base.boneWorldMatrices[i].Translation(),base.boneWorldMatrices[parent].Translation())
                - Vector3::Distance(sampled.boneWorldMatrices[i].Translation(),sampled.boneWorldMatrices[parent].Translation())) < 0.002f);
    }
    // 対象チェーン外のローカル姿勢は一切編集しない。腰・背中と反対側はワールドでも固定。
    for (auto fixedPart : {MotionBodyPart::Waist, MotionBodyPart::Spine,
        MotionBodyPart::RShoulder, MotionBodyPart::LShoulder, MotionBodyPart::RHipjoint, MotionBodyPart::LHipjoint})
    {
        const int bone = MotionSkeleton::FindModelBoneIndex(model, MotionSkeleton::GetBodyPartName(static_cast<int>(fixedPart)));
        Matrix before, after;
        MotionPose::GetBoneWorldMatrix(base, bone, object, before);
        MotionPose::GetBoneWorldMatrix(sampled, bone, object, after);
        assert(Vector3::Distance(before.Translation(), after.Translation()) < 0.002f);
        if (bone != result.boneIndices[0])
            for (int r=0;r<3;++r) for (int c=0;c<3;++c) assert(std::abs(before.m[r][c]-after.m[r][c]) < 0.002f);
    }
    // End orientation follows its parent; its own local rotation must not be changed by IK.
    assert(std::abs(sampled.bonePoses[endIndex].localRotation.Dot(base.bonePoses[endIndex].localRotation)) > 0.99999f);
    if (axis == 0 && amount == 0.1f)
    {
        // Only the test output directory is written; existing user motions are never saved here.
        const auto cwd = std::filesystem::current_path();
        std::filesystem::current_path(cwd / "x64/MotionIkTests");
        assert(MotionDataSaver::SaveMotionData("roundtrip",motion));
        MotionData restored;
        assert(MotionDataLoader::LoadMotionData("roundtrip",restored));
        std::filesystem::current_path(cwd);
        MotionPose::ApplyMotionData(sampled,restored,0,model,&base);
        MotionPose::UpdateSkinningMatrices(sampled,model);
        MotionPose::GetBoneWorldMatrix(sampled,endIndex,object,actual);
        assert(Vector3::Distance(actual.Translation(),result.reachedPosition) < 0.002f);
    }
    ++checks;
}

// Repeated edits include a release/re-grab, and then a no-op must preserve that exact pose.
static void CheckDrag(const ModelResource& model, SkeletonPose base, const Matrix& object, const MotionIkChain& chain)
{
    for (int axis = 0; axis < 3; ++axis)
    {
        const int end = MotionSkeleton::FindModelBoneIndex(model,MotionSkeleton::GetBodyPartName(static_cast<int>(chain.parts[2])));
        Matrix startMatrix;
        MotionPose::GetBoneWorldMatrix(base,end,object,startMatrix);
        MotionIkResult last;
        for (int step = 0; step < 240; ++step)
        {
            Vector3 target = startMatrix.Translation();
            (&target.x)[axis] += 0.3f*std::sin(step*0.025f);
            MotionIkResult current;
            assert(MotionIk::Solve(model,base,object,chain,target,current));
            if (step > 0)
            {
                assert(Vector3::Distance(last.reachedPosition,current.reachedPosition) < 0.015f);
                for (int i = 0; i < 3; ++i)
                    assert(std::abs(last.localRotations[i].Dot(current.localRotations[i])) > 0.95f);
            }
            last = current;
            ++checks;
        }
        for (int i = 0; i < 3; ++i) base.bonePoses[last.boneIndices[i]].localRotation = last.localRotations[i];
        MotionPose::UpdateSkinningMatrices(base,model);
        MotionPose::GetBoneWorldMatrix(base,end,object,startMatrix);
        MotionIkResult unchanged;
        assert(MotionIk::Solve(model,base,object,chain,startMatrix.Translation(),unchanged));
        for (int i=0;i<3;++i) assert(std::abs(last.localRotations[i].Dot(unchanged.localRotations[i])) > 0.99999f);
    }
}

// A solver that simply refuses every edit would satisfy invariants but is not usable.
// Shorten each straight limb, then verify actual movement and the permitted bend side.
static void CheckBending(const ModelResource& model, const SkeletonPose& base)
{
    for (const auto part : {MotionBodyPart::RHand, MotionBodyPart::LHand, MotionBodyPart::RFeet, MotionBodyPart::LFeet})
    {
        const auto& chain = *MotionIk::FindChain(part);
        const int root = MotionSkeleton::FindModelBoneIndex(model, MotionSkeleton::GetBodyPartName(static_cast<int>(chain.parts[0])));
        const int end = MotionSkeleton::FindModelBoneIndex(model, MotionSkeleton::GetBodyPartName(static_cast<int>(part)));
        const Vector3 start = base.boneWorldMatrices[end].Translation();
        const Vector3 target = Vector3::Lerp(start, base.boneWorldMatrices[root].Translation(), 0.1f);
        MotionIkResult result;
        assert(MotionIk::Solve(model, base, Matrix::Identity, chain, target, result));
        std::cout << "Bend " << MotionSkeleton::GetBodyPartName(static_cast<int>(part))
            << " moved=" << Vector3::Distance(start, result.reachedPosition) << " limit=" << result.atJointLimit << '\n';
        assert(Vector3::Distance(start, result.reachedPosition) > 0.1f);
        const int mid = result.boneIndices[1];
        const auto violation = MotionIk::MeasureLimitViolation(chain.parts[1], result.localRotations[1],
            testBones[mid].bindLocalRotation, base.bonePoses[mid].localRotation);
        assert(violation.Length() < 0.003f);
        ++checks;
    }
}

// The same predicate must govern direct rotation edits and IK, including invalid legacy poses.
static void CheckRotationEditing()
{
    const Quaternion bind = Quaternion::Identity;
    for (const auto part : {MotionBodyPart::RElbow, MotionBodyPart::LElbow, MotionBodyPart::RKnees, MotionBodyPart::LKnees,
        MotionBodyPart::RShoulder, MotionBodyPart::LShoulder, MotionBodyPart::RHipjoint, MotionBodyPart::LHipjoint})
    {
        for (const auto axis : {Vector3::UnitX, Vector3::UnitY, Vector3::UnitZ})
        {
            for (float initial : {-100.f, 0.f, 85.f, 95.f, 140.f})
            {
                const Quaternion start = Quaternion::CreateFromAxisAngle(axis, DirectX::XMConvertToRadians(initial));
                const auto range = MotionRotationLimits::AxisRange(part, start, axis, bind);
                const auto allowed = MotionRotationLimits::Measure(part, start, bind, start);
                for (float side : {-1.f, 1.f})
                {
                    const float boundary = side < 0 ? range.x : range.y;
                    Quaternion last = start;
                    for (int step = 0; step <= 180; ++step)
                    {
                        const float requested = side * DirectX::XMConvertToRadians(float(step));
                        const float angle = std::clamp(requested, range.x, range.y);
                        const auto rotation = Quaternion::CreateFromAxisAngle(axis, angle) * start;
                        assert(MotionRotationLimits::Permits(MotionRotationLimits::Measure(part, rotation, bind, start), allowed));
                        assert(std::abs(last.Dot(rotation)) > 0.999f);
                        last = rotation;
                    }
                    const auto edge = Quaternion::CreateFromAxisAngle(axis, boundary) * start;
                    const auto noChange = MotionRotationLimits::Constrain(part, edge, edge, bind);
                    assert(std::abs(edge.Dot(noChange)) > 0.99999f);
                    const auto target = Quaternion::CreateFromAxisAngle(axis, side * 2.0f) * start;
                    const auto constrained = MotionRotationLimits::Constrain(part, start, target, bind);
                    if (!MotionRotationLimits::Permits(MotionRotationLimits::Measure(part, constrained, bind, start), allowed))
                        std::cerr << "Rotation limit failed part=" << static_cast<int>(part) << " initial=" << initial << " side=" << side << '\n';
                    assert(MotionRotationLimits::Permits(MotionRotationLimits::Measure(part, constrained, bind, start), allowed));
                    ++checks;
                }
            }
        }
    }
    // Forearm pronation/supination is not the former 25 degree "other axes" cap.
    for (auto part : {MotionBodyPart::RElbow, MotionBodyPart::LElbow})
        for (float angle : {-90.f, 90.f})
            assert(MotionRotationLimits::Permits(MotionRotationLimits::Measure(part,
                Quaternion::CreateFromAxisAngle(Vector3::UnitY, DirectX::XMConvertToRadians(angle)), bind, bind), {}));
}

int main(int argc, char** argv)
{
    CheckRotationEditing();
    // Known rotations check the constraint itself independently of the IK solver.
    for (const auto part : {MotionBodyPart::RElbow, MotionBodyPart::LElbow, MotionBodyPart::RKnees, MotionBodyPart::LKnees})
    {
        const float sign = (part == MotionBodyPart::RElbow || part == MotionBodyPart::LElbow) ? -1.0f : 1.0f;
        const Quaternion bind = Quaternion::CreateFromYawPitchRoll(0.3f, 0.1f, -0.2f);
        const auto rotation = [&](float degrees)
        {
            return Quaternion::CreateFromRotationMatrix(Matrix::CreateRotationX(DirectX::XMConvertToRadians(degrees))
                * Matrix::CreateFromQuaternion(bind));
        };
        const auto valid = MotionIk::MeasureLimitViolation(part, rotation(sign * 80), bind, bind);
        const auto reversed = MotionIk::MeasureLimitViolation(part, rotation(-sign * 20), bind, bind);
        const auto excessive = MotionIk::MeasureLimitViolation(part, rotation(sign * 178), bind, bind);
        assert(valid.Length() < 0.002f);
        assert(std::abs(reversed.Length() - 20) < 0.002f);
        const auto limit = MotionSkeleton::GetPoseRotationLimit(part);
        const float maximum = sign < 0 ? -limit.minimum.x : limit.maximum.x;
        assert(excessive.Length() >= 178 - maximum - 0.002f);
        const auto negated = MotionIk::MeasureLimitViolation(part, -rotation(sign * 178), bind, bind);
        assert(Vector3::Distance(negated, excessive) < 0.002f);
        ++checks;
    }
    for (const char* path : {"DrectX11Sample/assets/model/DebugPlayer/man.fbx", "DrectX11Sample/assets/model/DebugPlayer2/woman.fbx"})
    {
        Assimp::Importer importer;
        const auto* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices
            | aiProcess_GenSmoothNormals | aiProcess_ConvertToLeftHanded | aiProcess_ImproveCacheLocality);
        assert(scene && scene->mRootNode);
        testBones.clear();
        ReadNodes(*scene->mRootNode, -1);
        ModelResource model;
        SkeletonPose pose;
        assert(MotionPose::InitializeSkeletonPose(pose, model, path));
        CheckBending(model, pose);
        // Verify that the actual FBX segment follows the local Y long axis assumed by the constraint.
        for (const auto part : {MotionBodyPart::RHand, MotionBodyPart::LHand, MotionBodyPart::RFeet, MotionBodyPart::LFeet})
        {
            const auto& chain = *MotionIk::FindChain(part);
            for (int i = 0; i < 2; ++i)
            {
                const int parent = MotionSkeleton::FindModelBoneIndex(model, MotionSkeleton::GetBodyPartName(static_cast<int>(chain.parts[i])));
                const int child = MotionSkeleton::FindModelBoneIndex(model, MotionSkeleton::GetBodyPartName(static_cast<int>(chain.parts[i+1])));
                Vector3 axis = Vector3::Transform(pose.boneWorldMatrices[child].Translation(), pose.boneWorldMatrices[parent].Invert());
                axis.Normalize();
                std::cout << "Long axis " << MotionSkeleton::GetBodyPartName(static_cast<int>(chain.parts[i])) << '=' << axis.y << '\n';
                assert(std::abs(axis.y) > 0.98f);
                ++checks;
            }
        }
        if (argc > 1 && std::string(argv[1]) == "--constraints-only") continue;
        std::cout << path << '\n';
        for (const auto& part : MotionSkeleton::GetBodyPartDefinitions())
        {
            int index = -1;
            for (const auto* name : part.modelBoneNames)
                if (name[0] && (index = model.FindBoneIndex(name)) >= 0) break;
            assert(index >= 0);
            auto pos = pose.boneWorldMatrices[index].Translation();
            std::cout << part.editorName << " -> " << testBones[index].name << " (" << pos.x << ',' << pos.y << ',' << pos.z << ") parents:";
            for (int p = testBones[index].parentIndex; p >= 0; p = testBones[p].parentIndex) std::cout << ' ' << testBones[p].name;
            std::cout << '\n';
        }
        for (int variant = 0; variant < 3; ++variant)
        {
            SkeletonPose base = pose;
            if (variant > 0)
            {
                for (const auto& part : MotionSkeleton::GetBodyPartDefinitions())
                {
                    const int index = MotionSkeleton::FindModelBoneIndex(model,part.editorName);
                    base.bonePoses[index].localRotation = Quaternion::CreateFromYawPitchRoll(0.23f*variant, -0.37f*variant, 0.19f*variant);
                }
                MotionPose::UpdateSkinningMatrices(base,model);
            }
            const Matrix object = Matrix::CreateScale(0.05f)*Matrix::CreateFromYawPitchRoll(-1.57f,0.14f,0.27f)*Matrix::CreateTranslation(3,1,-2);
            for (const auto& part : MotionSkeleton::GetBodyPartDefinitions())
                if (const auto* chain = MotionIk::FindChain(part.bodyPart))
                {
                    CheckDrag(model,base,object,*chain);
                    for (int axis = 0; axis < 3; ++axis)
                        for (float amount : {-100.f,-5.f,-1.f,-0.1f,-0.01f,0.f,0.01f,0.1f,1.f,5.f,100.f})
                            CheckSolve(model,base,object,*chain,axis,amount);
                }
        }
        // Exercise authored poses as well as the synthetic rotations above.
        for (const auto& file : std::filesystem::recursive_directory_iterator("DrectX11Sample/assets/MotionData"))
        {
            if (file.path().extension() != ".json") continue;
            MotionData motion;
            assert(MotionDataLoader::LoadMotionData(file.path().string(),motion));
            for (int frame : {0, motion.totalFrames/2, motion.totalFrames-1})
            {
                SkeletonPose authored = pose;
                MotionPose::ApplyMotionData(authored,motion,frame,model);
                MotionPose::UpdateSkinningMatrices(authored,model);
                for (const auto& part : MotionSkeleton::GetBodyPartDefinitions())
                    if (const auto* chain = MotionIk::FindChain(part.bodyPart))
                        for (int axis=0;axis<3;++axis)
                            for (float amount : {-0.1f,0.1f})
                                CheckSolve(model,authored,Matrix::CreateScale(0.05f)*Matrix::CreateRotationY(-1.57f),*chain,axis,amount);
            }
        }
    }
    if (argc == 1) assert(alternativeSolutions > 0);
    std::cout << "Alternative pose solutions: " << alternativeSolutions << '\n';
    std::cout << "IK FK/sampling/axis/reach tests passed: " << checks << '\n';
}
