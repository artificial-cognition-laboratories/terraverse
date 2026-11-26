// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <functional>
#include <mutex>
#include <vector>

namespace shell {

/*
	What Chromium's thread hands the game's: the page's intents, its
	navigations, its renderer's crashes. Chromium runs on a thread of its
	own (multi_threaded_message_loop), so nothing it does can stall a frame;
	everything that touches the engine is posted here and run in order at
	the frame's UI step (Shell::frame), on the main thread.
*/
class Inbox
{
public:
	// From any thread.
	void post(std::function<void()> task);
	// On the main thread: everything posted so far, in order. A task posting
	// another leaves it for the next frame.
	void run();

private:
	std::mutex m_lock;
	std::vector<std::function<void()>> m_tasks;
};

// Runs `task` on Chromium's UI thread and waits for it: for what CEF allows
// nowhere else (making a browser, a router's handlers). Start and shutdown
// only: the main thread waits on Chromium for it. Rethrows what it threw.
void onChromium(std::function<void()> task);

} // namespace shell
