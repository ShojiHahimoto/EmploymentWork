#pragma once

#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>

enum class LoadPriority
{
	Normal,
	SceneTransition
};

class LoadWorker
{
public:
	using Task = std::packaged_task<void()>;

	~LoadWorker();
	void Start();
	void Stop();
	void Submit(Task task, LoadPriority priority = LoadPriority::Normal);

	/// <summary>通常優先度で読み込みを依頼する。</summary>
	/// <typeparam name="Callable">読み込み処理の呼び出し可能型。</typeparam>
	/// <param name="work">ワーカーで実行する処理。</param>
	/// <returns>処理結果を受け取るfuture。未実行で破棄された場合はbroken promiseになる。</returns>
	template <class Callable>
	auto Enqueue(Callable work)
	{
		using Result = std::invoke_result_t<Callable &>;
		auto task = std::make_shared<std::packaged_task<Result()>>(std::move(work));
		auto result = task->get_future();
		Submit(Task([task]
		{
			(*task)();
		}));

		return result;
	}

private:
	void Run();

	std::mutex mutex;
	std::condition_variable ready;
	std::queue<Task> tasks;
	std::queue<Task> transitionTasks;
	std::thread thread;
	bool accepting = false;
};
