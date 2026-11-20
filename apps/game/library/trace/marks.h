// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes.h"
#include <string>
#include <string_view>
#include <vector>

/*
	Named spans of time worth keeping for good, for a waterfall: how the server
	started (its init, each mod's load). Few, and kept however long ago they
	were, unlike zones. `lane` says which waterfall a mark belongs to.
*/
namespace trace {

struct Mark
{
	u64 seq;
	std::string lane; // "startup"
	std::string what; // "Mods", "mod tf_world"
	u64 start, end;   // trace::now()
};

void mark(std::string_view lane, std::string_view what, u64 start, u64 end);
// Marks after `seq`, oldest first.
std::vector<Mark> marksSince(u64 seq);

// A mark from here to the end of the scope.
class Marking
{
public:
	Marking(std::string_view lane, std::string what);
	~Marking();
	Marking(const Marking &) = delete;
	Marking &operator=(const Marking &) = delete;

private:
	std::string m_lane, m_what;
	u64 m_start;
};

} // namespace trace
