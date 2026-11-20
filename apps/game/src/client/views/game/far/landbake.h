// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>

namespace far {

/*
	The wiki's Worlds time-lapse, baked with the palette: a river from a range to
	the sea on a real world (the home code), each erosion round of terragen's as
	it really ran (Grid's watcher), cut down to the cells of a small patch.
*/
Json::Value bakeRiverPatch();

} // namespace far
