#pragma once

#include <algorithm>
#include <vector>

namespace KeyframeEditing
{
    /// <summary>昇順を維持してキーを取得/追加する。保存前のプレビューも昇順を前提に検索する。</summary>
    /// <param name="keys">frameを持つ姿勢/移動/オフセットのキー配列。</param>
    /// <param name="frame">設定先の内部フレーム。</param>
    /// <returns>設定先のキー参照。後続の配列追加・削除をまたいで保持しない。</returns>
    template<class Key> Key& FindOrInsert(std::vector<Key>& keys, int frame)
    {
        const auto earlier = [](const Key& a, const Key& b) { return a.frame < b.frame; };
        // 過去の下書き等が未整列でも、lower_boundへ渡す前に前提を回復する。
        if (!std::is_sorted(keys.begin(), keys.end(), earlier)) std::stable_sort(keys.begin(), keys.end(), earlier);
        auto found = std::lower_bound(keys.begin(), keys.end(), frame,
            [](const Key& key, int value) { return key.frame < value; });
        if (found == keys.end() || found->frame != frame)
        {
            Key key;
            key.frame = frame;
            found = keys.insert(found, key);
        }
        return *found;
    }
}
