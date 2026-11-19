// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "terragen/parallel.h"
#include "trace/trace.h"
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace threading {

/*
	A few named workers and a queue: work handed off the main thread without a
	thread made for each piece (std::async made one every time; running about,
	hundreds of them came and went, and the debugger kept a rail for each).
	The workers are background threads (terragen::background) and show in the
	debugger as `name`, `name 2` and so on.

	Owned by whoever's work it does, declared after everything that work
	reads: going, it drops what hasn't started and waits for what has (a
	dropped job's future reports a broken promise when asked).
*/
class Pool
{
public:
	struct Opts
	{
		std::string name;
		unsigned threads;
	};

	explicit Pool(const Opts &opts)
	{
		for (unsigned i = 0; i < opts.threads; i++)
			m_workers.emplace_back([this, name = opts.name] { work(name); });
	}

	~Pool()
	{
		{
			std::lock_guard<std::mutex> guard(m_lock);
			m_stopping = true;
			m_queue.clear();
		}
		m_wake.notify_all();
		for (std::thread &t : m_workers)
			t.join();
	}

	Pool(const Pool &) = delete;
	Pool &operator=(const Pool &) = delete;

	// `fn` on a worker, in the order given; its result (or what it threw) by the future.
	template <typename F>
	std::future<std::invoke_result_t<F>> run(F &&fn)
	{
		using R = std::invoke_result_t<F>;
		auto task = std::make_shared<std::packaged_task<R()>>(std::forward<F>(fn));
		std::future<R> result = task->get_future();
		{
			std::lock_guard<std::mutex> guard(m_lock);
			m_queue.emplace_back([task] { (*task)(); });
		}
		m_wake.notify_one();
		return result;
	}

private:
	void work(const std::string &name)
	{
		trace::nameThread(name);
		terragen::background();
		for (;;) {
			std::function<void()> job;
			{
				std::unique_lock<std::mutex> guard(m_lock);
				m_wake.wait(guard, [this] { return m_stopping || !m_queue.empty(); });
				if (m_queue.empty())
					return; // stopping
				job = std::move(m_queue.front());
				m_queue.pop_front();
			}
			job();
		}
	}

	std::mutex m_lock;
	std::condition_variable m_wake;
	std::deque<std::function<void()>> m_queue;
	bool m_stopping = false;
	std::vector<std::thread> m_workers; // last: they start once the rest is made
};

} // namespace threading
