#pragma once

#include "Controller/CustomizeAttackEditorController.h"
#include "Data/MotionData.h"
#include <deque>
#include <optional>
#include <string>

/// <summary>Undo対象の下書き。リソースやカメラを複製せず、表示フレームだけを復元用に添える。</summary>
struct CustomizeEditSnapshot
{
    CustomizeAttackEditorEditState attack;
    MotionData motion;
    bool commonMotion = false;
    int previewFrame = 0; // 比較対象ではなく、その操作を確認するための表示位置。
};

/// <summary>画面や入力ライブラリに依存しない、技・モーション共通の編集履歴。</summary>
class CustomizeEditHistory
{
public:
    static constexpr size_t Capacity = 100; // 戻せる操作数はここだけで変更する。
    static constexpr double WheelMergeSeconds = 0.3; // UI用の実時間。ゲームの固定フレームとは別。

    /// <summary>別スロットへの移動などで履歴を破棄する。</summary>
    void Reset();
    /// <summary>編集前後を記録する。同じ入力が継続中なら開始状態だけを保ち、途中経過を置き換える。</summary>
    /// <param name="before">今回の変更直前。</param>
    /// <param name="after">今回の変更直後。</param>
    /// <param name="operation">同じドラッグ・入力欄を識別するID。</param>
    /// <param name="ongoing">ドラッグや文字入力が継続中ならtrue。</param>
    /// <param name="wheel">短時間の同一欄ホイール入力をまとめる場合true。</param>
    /// <param name="now">単調増加するUI時刻（秒）。</param>
    void Observe(const CustomizeEditSnapshot& before, const CustomizeEditSnapshot& after,
        const std::string& operation, bool ongoing, bool wheel, double now);
    /// <summary>未確定の操作を履歴へ確定する。開始前と同じなら記録しない。</summary>
    void Commit();
    /// <summary>編集中の操作も確定してから一つ戻す。ファイルの保存状態は変更しない。</summary>
    /// <param name="result">復元する下書き。</param>
    /// <returns>復元対象があればtrue。</returns>
    bool Undo(CustomizeEditSnapshot& result);
    /// <summary>取り消した操作を一つやり直す。</summary>
    /// <param name="result">復元する下書き。</param>
    /// <returns>復元対象があればtrue。</returns>
    bool Redo(CustomizeEditSnapshot& result);
    /// <summary>未確定操作も含め、戻せる変更があるか返す。</summary>
    /// <returns>Undo可能ならtrue。</returns>
    bool CanUndo() const;
    /// <summary>新しい編集が始まっていない場合だけRedo可能とする。</summary>
    /// <returns>Redo可能ならtrue。</returns>
    bool CanRedo() const;
    /// <summary>指定操作が編集中か確認する。UI更新がGizmoの履歴を分断しないために使う。</summary>
    /// <param name="operation">操作ID。</param>
    /// <returns>同じ操作の開始状態を保持していればtrue。</returns>
    bool IsPending(const std::string& operation) const;
    /// <summary>保存対象の値を比較する。表示フレームやEuler表示用キャッシュは比較しない。</summary>
    /// <param name="a">比較元。</param>
    /// <param name="b">比較先。</param>
    /// <returns>下書きの内容が同じならtrue。</returns>
    static bool Equal(const CustomizeEditSnapshot& a, const CustomizeEditSnapshot& b);

private:
    struct Entry { CustomizeEditSnapshot before, after; };
    std::deque<Entry> entries;
    size_t cursor = 0; // 次にRedoする位置。0..entries.size()。
    std::optional<Entry> pending;
    std::string pendingOperation;
    bool pendingWheel = false;
    double lastWheelTime = 0;
};
