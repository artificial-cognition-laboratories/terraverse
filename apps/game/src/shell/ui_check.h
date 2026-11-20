// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irrlichttypes.h"
#include <dimension2d.h>
#include <functional>
#include <memory>

struct SEvent;

namespace shell {

/*
	The UI held to its budgets (bun check:ui, --ui-check): on the menu, the
	mouse swept over the page every frame and its lists scrolled, through
	the same path as the player's input, then what the frames cost judged
	against a table of budgets: the main thread's UI time, input to screen,
	the page's frame gaps. Prints "UICHECK …" lines, the last "UICHECK pass"
	or "UICHECK fail", and quits the game.
*/
class UiCheck
{
public:
	struct Opts
	{
		std::function<bool(const SEvent &)> input; // as the window's events go (Shell::onEvent)
		std::function<bool()> ready;               // the page has said it's ready
	};
	explicit UiCheck(Opts opts);
	~UiCheck();

	// Once a frame, its end (Shell::frame): the next move, and the frames
	// ended since counted. `screen`: the page's size.
	void step(core::dimension2du screen);

	struct Stats;

private:
	Opts m_opts;
	std::unique_ptr<Stats> m_stats;
	u64 m_ready_at = 0, m_last_frame = 0;
	u32 m_moves = 0;
	bool m_done = false;
};

} // namespace shell
