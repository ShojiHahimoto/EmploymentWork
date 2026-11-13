#include "Controller/CustomizeEditHistory.h"
#include <cassert>
#include "Controller/KeyframeEditing.h"
#include <iostream>

int main()
{
    // 保存処理を呼ばず、途中/先頭/末尾への追加と同じフレームの更新を検証する。
    std::vector<MotionBoneKeyframeData> poseKeys;
    std::vector<MotionRootOffsetKeyData> offsetKeys;
    std::vector<AttackMovementKeyData> moveKeys;
    for (int frame : {10, 20, 5, 0, 15, 30})
    {
        KeyframeEditing::FindOrInsert(poseKeys, frame).hasRotation = true;
        KeyframeEditing::FindOrInsert(offsetKeys, frame).offset = {static_cast<float>(frame), 0, 0};
        KeyframeEditing::FindOrInsert(moveKeys, frame).offset = {static_cast<float>(frame), 0};
    }
    const int expected[] = {0, 5, 10, 15, 20, 30};
    for (size_t i = 0; i < 6; ++i)
        assert(poseKeys[i].frame == expected[i] && offsetKeys[i].frame == expected[i] && moveKeys[i].frame == expected[i]);
    auto& pasted = KeyframeEditing::FindOrInsert(poseKeys, 5);
    pasted.localRotation = DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(0.1f, 0.2f, 0.3f);
    assert(poseKeys.size() == 6 && poseKeys[1].localRotation == pasted.localRotation);
    KeyframeEditing::FindOrInsert(moveKeys, 5).offset = {50, 60};
    KeyframeEditing::FindOrInsert(offsetKeys, 5).offset = {50, 60, 70};
    assert(moveKeys.size() == 6 && moveKeys[1].offset.x == 50 && offsetKeys[1].offset.z == 70);
    // 過去の未整列下書きを渡しても、設定時点で復旧し、同じフレームを重複追加しない。
    std::reverse(poseKeys.begin(), poseKeys.end());
    KeyframeEditing::FindOrInsert(poseKeys, 5).hasRotation = true;
    assert(poseKeys.size() == 6 && poseKeys[1].frame == 5);

    CustomizeEditHistory history;
    CustomizeEditSnapshot base, restored;
    base.motion.motionDataId = "Attack/Ground/slot_00";
    base.previewFrame = 8;
    auto changed = base;
    changed.attack.attackData.damage = 100;
    history.Observe(base, changed, "text/damage", true, false, 0);
    assert(history.CanUndo() && !history.CanRedo());
    auto final = changed;
    final.attack.attackData.damage = 200;
    history.Observe(changed, final, "text/damage", true, false, 1);
    history.Observe(final, final, "", false, false, 2);
    assert(history.Undo(restored) && CustomizeEditHistory::Equal(restored, base) && restored.previewFrame == 8);
    assert(!history.Undo(restored)); // 2フレームの更新を一操作にまとめる。
    assert(history.Redo(restored) && CustomizeEditHistory::Equal(restored, final));
    assert(!history.Redo(restored));

    history.Reset();
    history.Observe(base, changed, "gizmo", true, false, 0);
    history.Observe(changed, base, "gizmo", false, false, 1);
    assert(!history.CanUndo()); // 元の値へ戻したドラッグは履歴を作らない。
    auto selected = base;
    selected.previewFrame = 20;
    history.Observe(base, selected, "", false, false, 2);
    assert(!history.CanUndo()); // シークは編集ではない。

    history.Observe(base, changed, "wheel/damage", false, true, 0);
    history.Observe(changed, changed, "", false, false, 0.1);
    history.Observe(changed, final, "wheel/damage", false, true, 0.2);
    history.Observe(final, final, "", false, false, 0.6);
    assert(history.Undo(restored) && CustomizeEditHistory::Equal(restored, base));
    assert(!history.Undo(restored));
    assert(history.Redo(restored) && CustomizeEditHistory::Equal(restored, final));
    history.Undo(restored);
    history.Observe(base, changed, "gizmo", true, false, 1);
    assert(!history.CanRedo());
    history.Observe(changed, base, "gizmo", false, false, 2);
    assert(history.CanRedo()); // 無変更の操作はRedoを消さない。
    history.Observe(base, changed, "button", false, false, 3);
    assert(!history.CanRedo());

    history.Reset();
    history.Observe(base, changed, "wheel/damage", false, true, 0);
    history.Observe(changed, final, "wheel/damage", false, true, 1);
    assert(history.Undo(restored) && CustomizeEditHistory::Equal(restored, changed));
    assert(history.Undo(restored) && CustomizeEditHistory::Equal(restored, base));

    history.Reset();
    auto current = base;
    for (int i = 0; i < 120; ++i)
    {
        auto next = current;
        next.attack.attackData.damage = 1000 + i;
        history.Observe(current, next, "step", false, false, i);
        current = next;
    }
    int count = 0;
    while (history.Undo(restored)) ++count;
    assert(count == CustomizeEditHistory::Capacity);

    // 多関節の貼り付け・プリセット・IK結果も一つのSnapshotで復元する。
    history.Reset();
    auto pose = base;
    MotionBoneTrackData track;
    track.boneName = "RShoulder";
    MotionBoneKeyframeData key;
    key.frame = 7; key.hasRotation = true;
    key.localRotation = DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(0.2f, 0.3f, 0.4f);
    track.keyframes.push_back(key);
    pose.motion.boneTracks.push_back(track);
    track.boneName = "RElbow";
    pose.motion.boneTracks.push_back(track);
    pose.motion.rootOffsetKeys.push_back({7, {1, 2, 3}});
    pose.attack.attackData.movementKeys.push_back({7, {4, 5}});
    pose.attack.attackData.hitboxes.push_back({{6, 7}, {8, 9}});
    pose.attack.attackData.canAttackCancel = true;
    pose.attack.attackData.cancelSetting.cancelTypes.push_back(AttackCancelType::Special);
    history.Observe(base, pose, "preset", false, false, 0);
    assert(history.Undo(restored) && CustomizeEditHistory::Equal(base, restored));
    assert(history.Redo(restored) && CustomizeEditHistory::Equal(pose, restored));
    auto displayOnly = pose;
    displayOnly.motion.boneTracks[0].keyframes[0].localRotationEulerDegrees.x += 360;
    assert(CustomizeEditHistory::Equal(pose, displayOnly));
    auto equivalent = pose;
    auto& q = equivalent.motion.boneTracks[0].keyframes[0].localRotation;
    q.x = -q.x; q.y = -q.y; q.z = -q.z; q.w = -q.w;
    assert(CustomizeEditHistory::Equal(pose, equivalent)); // Quaternionの符号だけの差は同じ姿勢。
    auto commonA = base, commonB = changed;
    commonA.commonMotion = commonB.commonMotion = true;
    assert(CustomizeEditHistory::Equal(commonA, commonB));
    commonB.motion.looping = true;
    assert(!CustomizeEditHistory::Equal(commonA, commonB));
    history.Reset();
    assert(!history.CanUndo() && !history.CanRedo());
    std::cout << "Customize edit history tests passed\n";
}
