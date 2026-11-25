#pragma once

#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>

class SaveWorker
{
public:
	using Task = std::packaged_task<void()>;

	~SaveWorker();
	void Start();
	void Stop();

	/// <summary>保存を受付順のキューに登録する。</summary>
	/// <typeparam name="Callable">保存処理の呼び出し可能型。</typeparam>
	/// <param name="work">保存ワーカーで実行する処理。</param>
	/// <returns>保存結果を受け取るfuture。受付終了後はbroken promiseになる。</returns>
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
	void Submit(Task task);
	void Run();

	std::mutex mutex;
	std::condition_variable ready;
	std::queue<Task> tasks;
	std::thread thread;
	bool accepting = false;
};
