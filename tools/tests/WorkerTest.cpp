#include "Manager/LoadWorker.h"
#include "Manager/SaveWorker.h"

#include <cassert>
#include <vector>

int main()
{
	LoadWorker loader;
	loader.Start();
	std::promise<void> releaseLoad;
	std::promise<void> loadStarted;
	auto gate = releaseLoad.get_future();
	auto running = loader.Enqueue([&]
	{
		loadStarted.set_value();
		gate.wait();
	});
	loadStarted.get_future().wait();

	std::vector<int> loadOrder;
	auto normal = loader.Enqueue([&]
	{
		loadOrder.push_back(2);
	});
	loader.Submit(std::packaged_task<void()>([&]
	{
		loadOrder.push_back(1);
	}), LoadPriority::SceneTransition);
	releaseLoad.set_value();
	running.get();
	normal.get();
	loader.Stop();
	assert((loadOrder == std::vector<int>{1, 2}));

	loader.Start();
	std::promise<void> releaseRunning;
	std::promise<void> runningStarted;
	auto active = loader.Enqueue([&]
	{
		runningStarted.set_value();
		releaseRunning.get_future().wait();
	});
	runningStarted.get_future().wait();

	auto skipped = loader.Enqueue([] {});
	auto stopping = std::async(std::launch::async, [&]
	{
		loader.Stop();
	});
	skipped.wait();
	releaseRunning.set_value();
	active.get();
	stopping.get();

	try
	{
		skipped.get();
		assert(false);
	}
	catch (const std::future_error &error)
	{
		assert(error.code() == std::make_error_code(std::future_errc::broken_promise));
	}

	SaveWorker saver;
	saver.Start();
	std::vector<int> saveOrder;
	auto first = saver.Enqueue([&]
	{
		saveOrder.push_back(1);
	});
	auto second = saver.Enqueue([&]
	{
		saveOrder.push_back(2);
	});
	saver.Stop();
	first.get();
	second.get();
	assert((saveOrder == std::vector<int>{1, 2}));
}
