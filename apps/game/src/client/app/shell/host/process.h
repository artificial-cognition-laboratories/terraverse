// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes.h"
#include <json/json.h>
#include <memory>

namespace shell {

/*
	The game process as the debugger's Server tab shows it: CPU (of all its
	threads, as a share of one core) and resident memory, and on Linux each
	thread's CPU by name. Sampled: each call measures since the last, so call
	it at a steady rate.
*/
class Process
{
public:
	Process();
	~Process();

	// {pid, cpu (%), rss (bytes), threads: [{name, cpu}]} (threads where the OS says)
	Json::Value sample();

private:
	struct Impl;
	std::unique_ptr<Impl> m;
};

} // namespace shell
