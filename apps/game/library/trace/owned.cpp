// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/trace/owned.h"
#include "library/trace/trace.h"
#include <map>
#include <mutex>

namespace trace {

namespace {

// The recorder's kind of state: any thread reports, the debugger reads.
std::mutex g_lock;
std::map<std::string, Owned, std::less<>> g_owners;

} // namespace

void owned(std::string_view name, u64 bytes, u64 count, Where where)
{
	const u64 at = now();
	std::lock_guard<std::mutex> lock(g_lock);
	auto it = g_owners.find(name);
	if (it == g_owners.end())
		it = g_owners.emplace(std::string(name), Owned{ std::string(name), 0, 0, where, 0 }).first;
	it->second.bytes = bytes;
	it->second.count = count;
	it->second.where = where;
	it->second.at = at;
}

void disowned(std::string_view name)
{
	std::lock_guard<std::mutex> lock(g_lock);
	auto it = g_owners.find(name);
	if (it != g_owners.end())
		g_owners.erase(it);
}

std::vector<Owned> owners()
{
	std::lock_guard<std::mutex> lock(g_lock);
	std::vector<Owned> out;
	out.reserve(g_owners.size());
	for (const auto &[name, o] : g_owners)
		out.push_back(o);
	return out;
}

} // namespace trace
