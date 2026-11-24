#pragma once

#include <vector>

namespace KeyframeSearch
{
	template <class Key>
	struct Neighbors
	{
		const Key *previous;
		const Key *next;
	};

	/// <summary>指定フレームの前後にあるキーを探す。完全一致なら両方に同じキーを返す。</summary>
	/// <param name="keys">frame の昇順に並んだキー配列。</param>
	/// <param name="frame">検索する内部フレーム。</param>
	/// <returns>前後のキー。存在しない側は nullptr。配列の変更で無効になり得る。</returns>
	template <class Key>
	Neighbors<Key> FindNeighbors(const std::vector<Key> &keys, int frame)
	{
		Neighbors<Key> result{nullptr, nullptr};

		for (const Key &key : keys)
		{
			if (key.frame <= frame)
			{
				result.previous = &key;
			}

			if (key.frame >= frame)
			{
				result.next = &key;
				break;
			}
		}

		return result;
	}
}
