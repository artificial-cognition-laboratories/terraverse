// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/host/inbox.h"
#include "src/shared/base/exceptions.h"
#include <include/base/cef_callback.h>
#include <include/cef_task.h>
#include <include/wrapper/cef_closure_task.h>
#include <exception>
#include <future>

namespace shell {

void Inbox::post(std::function<void()> task)
{
	std::lock_guard<std::mutex> guard(m_lock);
	m_tasks.push_back(std::move(task));
}

void Inbox::run()
{
	std::vector<std::function<void()>> tasks;
	{
		std::lock_guard<std::mutex> guard(m_lock);
		tasks.swap(m_tasks);
	}
	for (const auto &task : tasks)
		task();
}

void onChromium(std::function<void()> task)
{
	if (CefCurrentlyOn(TID_UI)) {
		task();
		return;
	}
	std::promise<void> done;
	std::future<void> result = done.get_future();
	const bool posted = CefPostTask(TID_UI, CefCreateClosureTask(base::BindOnce(
		[](std::function<void()> *task, std::promise<void> *done) {
			try {
				(*task)();
				done->set_value();
			} catch (...) {
				done->set_exception(std::current_exception());
			}
		}, &task, &done)));
	if (!posted)
		throw BaseException("shell: Chromium's UI thread took no task (not running?)");
	result.get();
}

} // namespace shell
