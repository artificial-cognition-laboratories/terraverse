// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes.h"
#include <string>
#include <string_view>
#include <vector>

/*
	Who holds the memory: each owner says what it holds now, by name, as its
	size changes (or each frame, if that's simpler: only the latest is kept).
	The debugger's Memory tab samples these once a second beside the process's
	own figures; what no owner claims shows there as unaccounted, which is
	where a leak in code nobody measured appears.

	Names are "System: what" ("Far view: tiles"), as zones are: the Memory tab
	charts each system (the part before the colon) as one. `count` is whatever
	an owner holds a number of (tiles, blocks, meshes); 0 when none. `where`:
	the machine's memory, or the graphics card's.
*/
namespace trace {

enum class Where : u8 { Ram, Gpu };

void owned(std::string_view name, u64 bytes, u64 count, Where where = Where::Ram);
// The owner's gone: its figure with it.
void disowned(std::string_view name);

struct Owned
{
	std::string name;
	u64 bytes, count;
	Where where;
	u64 at; // trace::now() when it was last said
};

// Every owner's latest, by name.
std::vector<Owned> owners();

} // namespace trace
