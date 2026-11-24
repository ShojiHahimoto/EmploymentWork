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

/// <summary>Undo、Redo用の、技・モーション共通の編集履歴。</summary>
class CustomizeEditHistory
{
  public:
	static constexpr size_t Capacity = 100;			 // 戻せる操作数はここだけで変更する。
	static constexpr double WheelMergeSeconds = 0.3; // UI用の実時間。ゲームの固定フレームとは別。

	void Reset();
	void Observe(const CustomizeEditSnapshot &before, const CustomizeEditSnapshot &after, const std::string &operation,
				 bool ongoing, bool wheel, double now);
	void Commit();
	bool Undo(CustomizeEditSnapshot &result);
	bool Redo(CustomizeEditSnapshot &result);
	bool CanUndo() const;
	bool CanRedo() const;
	bool IsPending(const std::string &operation) const;
	static bool Equal(const CustomizeEditSnapshot &a, const CustomizeEditSnapshot &b);

  private:
	struct Entry
	{
		CustomizeEditSnapshot before, after;
	};
	std::deque<Entry> entries;
	size_t cursor = 0; // 次にRedoする位置。0..entries.size()。
	std::optional<Entry> pending;
	std::string pendingOperation;
	bool pendingWheel = false;
	double lastWheelTime = 0;
};
