#include "Manager/SaveWorker.h"

/// <summary>受付済みの保存を完了させてから破棄する。</summary>
SaveWorker::~SaveWorker()
{
	Stop();
}

/// <summary>保存専用スレッドを起動する。</summary>
void SaveWorker::Start()
{
	std::lock_guard lock(mutex);
	if (thread.joinable())
	{
		return;
	}

	thread = std::thread([this]
	{
		Run();
	});
	accepting = true;
}

/// <summary>新規受付を閉じ、受付済みの保存を順番に完了してから停止する。</summary>
void SaveWorker::Stop()
{
	{
		std::lock_guard lock(mutex);
		accepting = false;
	}

	ready.notify_one();
	if (thread.joinable())
	{
		thread.join();
	}
}

/// <summary>保存を受付順のキューに追加する。受付終了後は実行しない。</summary>
/// <param name="task">別スレッドで実行する保存処理。</param>
void SaveWorker::Submit(Task task)
{
	{
		std::lock_guard lock(mutex);
		if (!accepting)
		{
			return;
		}

		tasks.push(std::move(task));
	}

	ready.notify_one();
}

/// <summary>保存を受付順に実行し、受付終了後はキューが空になった時点で終了する。</summary>
void SaveWorker::Run()
{
	for (;;)
	{
		Task task;
		{
			std::unique_lock lock(mutex);
			ready.wait(lock, [this]
			{
				return !accepting || !tasks.empty();
			});

			if (tasks.empty())
			{
				return;
			}

			task = std::move(tasks.front());
			tasks.pop();
		}

		task();
	}
}
