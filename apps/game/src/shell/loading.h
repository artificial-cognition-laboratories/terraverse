// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>
#include <string>

/*
	A world being made, as the page shows it (ui/src/bridge/types.ts: Loading):
	terragen's steps (terragen/progress.h) gathered into a few stages a player
	can read, each weighed by how long it takes made afresh, so the bar moves
	with time rather than by steps. A stage whose work was kept, never run, is
	shown kept.
*/
namespace shell {

// How far the world of `code` is from shown: only steps begun since `since`
// (terragen::clock()) count, and any still running
Json::Value loadingOf(const std::string &code, double since);

} // namespace shell
