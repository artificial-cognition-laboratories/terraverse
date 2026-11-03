// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "parallel.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

namespace terragen {

namespace {

thread_local bool t_in_part = false;

// Threads that live with the program (detached: they hold nothing to tear
// down, and a world may be building as it exits).
class Crew
{
public:
	Crew()
	{
		const unsigned cores = std::max(1u, std::thread::hardware_concurrency());
		for (unsigned k = 1; k < cores; k++)
			std::thread([this] { work(); }).detach();
		m_helpers = (int)cores - 1;
	}

	void run(int32_t n, const std::function<void(int32_t, int32_t, int32_t)> &fn)
	{
		std::unique_lock<std::mutex> turn(m_turn, std::try_to_lock);
		if (t_in_part || !turn.owns_lock() || m_helpers == 0) {
			serial(n, fn);
			return;
		}
		{
			std::lock_guard<std::mutex> hold(m_lock);
			m_fn = &fn;
			m_n = n;
			m_next = 0;
			m_error = nullptr;
			m_out = m_helpers;
			m_job++;
		}
		m_wake.notify_all();
		parts();
		std::unique_lock<std::mutex> hold(m_lock);
		m_done.wait(hold, [this] { return m_out == 0; });
		if (m_error)
			std::rethrow_exception(m_error);
	}

private:
	static void serial(int32_t n, const std::function<void(int32_t, int32_t, int32_t)> &fn)
	{
		for (int32_t p = 0; p < PARTS; p++)
			fn(p, (int32_t)((int64_t)n * p / PARTS), (int32_t)((int64_t)n * (p + 1) / PARTS));
	}

	void parts()
	{
		t_in_part = true;
		for (int32_t p; (p = m_next.fetch_add(1)) < PARTS;) {
			try {
				(*m_fn)(p, (int32_t)((int64_t)m_n * p / PARTS), (int32_t)((int64_t)m_n * (p + 1) / PARTS));
			} catch (...) {
				std::lock_guard<std::mutex> hold(m_lock);
				if (!m_error)
					m_error = std::current_exception();
			}
		}
		t_in_part = false;
	}

	void work()
	{
		uint64_t seen = 0;
		std::unique_lock<std::mutex> hold(m_lock);
		while (true) {
			m_wake.wait(hold, [&] { return m_job != seen; });
			seen = m_job;
			hold.unlock();
			parts();
			hold.lock();
			if (--m_out == 0)
				m_done.notify_all();
		}
	}

	std::mutex m_turn; // one loop at a time
	std::mutex m_lock;
	std::condition_variable m_wake, m_done;
	const std::function<void(int32_t, int32_t, int32_t)> *m_fn = nullptr;
	int32_t m_n = 0;
	std::atomic<int32_t> m_next{ 0 };
	std::exception_ptr m_error;
	int m_out = 0, m_helpers = 0;
	uint64_t m_job = 0;
};

} // namespace

void parallelParts(int32_t n, const std::function<void(int32_t, int32_t, int32_t)> &fn)
{
	// never destroyed: its threads wait on it to the end of the program
	static Crew &crew = *new Crew;
	crew.run(n, fn);
}

} // namespace terragen
