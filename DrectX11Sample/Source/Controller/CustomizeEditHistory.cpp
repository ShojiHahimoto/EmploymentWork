#include "Controller/CustomizeEditHistory.h"
#include <tuple>

namespace
{
    /// <summary>配列の各値を構造化比較し、paddingやポインタの値を比較しない。</summary>
    /// <param name="a">比較元。</param>
    /// <param name="b">比較先。</param>
    /// <param name="equal">要素の比較。</param>
    /// <returns>順序を含めて一致すればtrue。</returns>
    template<class T, class F> bool EqualItems(const std::vector<T>& a, const std::vector<T>& b, F equal)
    {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) if (!equal(a[i], b[i])) return false;
        return true;
    }

    /// <summary>技データの保存対象と編集用硬直差を比較する。</summary>
    /// <param name="a">比較元。</param>
    /// <param name="b">比較先。</param>
    /// <returns>一致すればtrue。</returns>
    bool EqualAttack(const CustomizeAttackEditorEditState& a, const CustomizeAttackEditorEditState& b)
    {
        const auto fields = [](const CustomizeAttackEditorEditState& s)
        {
            const auto& d = s.attackData;
            return std::tie(s.hitFrameAdvantage, s.guardFrameAdvantage, d.attackDataId, d.displayName,
                d.damage, d.hitstunFrames, d.guardstunFrames, d.attackKind, d.commandId, d.usableState,
                d.attackHeight, d.hitReactionType, d.motionDataId, d.hitSoundId,
                d.frame.startup, d.frame.active, d.frame.recovery, d.canAttackCancel,
                d.cancelSetting.startFrame, d.cancelSetting.endFrame, d.cancelSetting.cancelTypes);
        };
        return fields(a) == fields(b)
            && EqualItems(a.attackData.hitboxes, b.attackData.hitboxes, [](const auto& x, const auto& y) { return x.offset == y.offset && x.size == y.size; })
            && EqualItems(a.attackData.movementKeys, b.attackData.movementKeys, [](const auto& x, const auto& y) { return x.frame == y.frame && x.offset == y.offset; });
    }
}

bool CustomizeEditHistory::Equal(const CustomizeEditSnapshot& a, const CustomizeEditSnapshot& b)
{
    if (a.commonMotion != b.commonMotion || (!a.commonMotion && !EqualAttack(a.attack, b.attack))) return false;
    const auto& x = a.motion;
    const auto& y = b.motion;
    if (std::tie(x.motionDataId, x.displayName, x.totalFrames, x.looping) != std::tie(y.motionDataId, y.displayName, y.totalFrames, y.looping)) return false;
    return EqualItems(x.rootOffsetKeys, y.rootOffsetKeys, [](const auto& p, const auto& q) { return p.frame == q.frame && p.offset == q.offset; })
        && EqualItems(x.boneTracks, y.boneTracks, [](const auto& p, const auto& q)
        {
            return p.boneName == q.boneName && EqualItems(p.keyframes, q.keyframes, [](const auto& k, const auto& l)
            {
                return k.frame == l.frame && k.hasPosition == l.hasPosition && k.localPosition == l.localPosition
                    && k.hasRotation == l.hasRotation
                    && (k.localRotation == l.localRotation
                        || (k.localRotation.x == -l.localRotation.x && k.localRotation.y == -l.localRotation.y
                            && k.localRotation.z == -l.localRotation.z && k.localRotation.w == -l.localRotation.w))
                    && k.hasScale == l.hasScale && k.localScale == l.localScale;
            });
        });
}

void CustomizeEditHistory::Reset()
{
    entries.clear(); cursor = 0; pending.reset(); pendingOperation.clear(); pendingWheel = false;
}

void CustomizeEditHistory::Observe(const CustomizeEditSnapshot& before, const CustomizeEditSnapshot& after,
    const std::string& operation, bool ongoing, bool wheel, double now)
{
    const bool changed = !Equal(before, after);
    // 空の更新はホイールの待ち時間だけを進める。他の編集開始は即座に区切る。
    const bool wheelWaiting = pendingWheel && operation.empty() && !changed && now - lastWheelTime < WheelMergeSeconds;
    if (wheelWaiting) return;
    if (pending && (pendingOperation != operation || (pendingWheel && now - lastWheelTime >= WheelMergeSeconds))) Commit();
    if (changed)
    {
        if (!pending) { pending = Entry{before, after}; pendingOperation = operation; }
        else pending->after = after;
        pendingWheel = wheel;
        if (wheel) lastWheelTime = now;
    }
    if (!ongoing && !wheel) Commit();
}

void CustomizeEditHistory::Commit()
{
    if (pending && !Equal(pending->before, pending->after))
    {
        // Redoは本当に新しい変更が確定した時だけ破棄する。値を元に戻した入力では破棄しない。
        entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(cursor), entries.end());
        entries.push_back(std::move(*pending));
        if (entries.size() > Capacity) entries.pop_front();
        cursor = entries.size();
    }
    pending.reset(); pendingOperation.clear(); pendingWheel = false;
}

bool CustomizeEditHistory::Undo(CustomizeEditSnapshot& result)
{
    Commit();
    if (cursor == 0) return false;
    result = entries[--cursor].before;
    return true;
}

bool CustomizeEditHistory::Redo(CustomizeEditSnapshot& result)
{
    Commit();
    if (cursor == entries.size()) return false;
    result = entries[cursor++].after;
    return true;
}

bool CustomizeEditHistory::CanUndo() const { return cursor > 0 || (pending && !Equal(pending->before, pending->after)); }
bool CustomizeEditHistory::CanRedo() const { return cursor < entries.size() && (!pending || Equal(pending->before, pending->after)); }
bool CustomizeEditHistory::IsPending(const std::string& operation) const { return pending && pendingOperation == operation; }
