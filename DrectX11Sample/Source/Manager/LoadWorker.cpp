#include "Manager/LoadWorker.h"

#include <objbase.h>

/// <summary>未着手の読み込みを破棄し、実行中の処理を待って破棄する。</summary>
LoadWorker::~LoadWorker()
{
	Stop();
}

/// <summary>ロードスレッドを起動し、WICに必要なCOMをそのスレッドで初期化する。</summary>
void LoadWorker::Start()
{
	std::lock_guard lock(mutex);
	if (thread.joinable())
	{
		return;
	}

	thread = std::thread([this]
	{
		const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		Run();

		if (SUCCEEDED(com))
		{
			CoUninitialize();
		}
	});
	accepting = true;
}

/// <summary>未着手のロードを破棄し、実行中の処理を待って停止する。</summary>
void LoadWorker::Stop()
{
	{
		std::lock_guard lock(mutex);
		accepting = false;
		tasks = {};
		transitionTasks = {};
	}

	ready.notify_one();
	if (thread.joinable())
	{
		thread.join();
	}
}

/// <summary>未着手キューに読み込みを追加する。受付終了後は実行しない。</summary>
/// <param name="task">別スレッドで実行する読み込み処理。</param>
/// <param name="priority">通常の読み込みか、先に実行するシーン遷移か。</param>
void LoadWorker::Submit(Task task, LoadPriority priority)
{
	{
		std::lock_guard lock(mutex);
		if (!accepting)
		{
			return;
		}

		auto &queue = priority == LoadPriority::SceneTransition ? transitionTasks : tasks;
		queue.push(std::move(task));
	}

	ready.notify_one();
}

/// <summary>キューを待機し、未着手の通常ロードよりシーン遷移を優先して実行する。</summary>
void LoadWorker::Run()
{
	for (;;)
	{
		Task task;
		{
			std::unique_lock lock(mutex);
			ready.wait(lock, [this]
			{
				return !accepting || !transitionTasks.empty() || !tasks.empty();
			});

			if (transitionTasks.empty() && tasks.empty())
			{
				return;
			}

			auto &queue = transitionTasks.empty() ? tasks : transitionTasks;
			task = std::move(queue.front());
			queue.pop();
		}

		task();
	}
}
