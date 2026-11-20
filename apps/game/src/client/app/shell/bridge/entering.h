// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>
#include <optional>
#include <vector>

namespace video { class IVideoDriver; }
namespace gui { class ICursorControl; }

namespace shell {

class Shell;

/*
	A world being entered, from Play to its first frame: the planet the menu
	showed stays on screen (Planetarium::hold), the page over it, and the page
	is told how far it's got by stage, as the planetarium's making is
	(ui/src/bridge/types.ts: Loading, sent as { kind: "entering", loading }).

	The engine's own load screen (RenderingEngine::draw_load_screen) is a frame
	here: its progress (0..100, the steps upstream gives each) says which stage
	is running.
*/
class Entering
{
public:
	struct Opts
	{
		Shell &shell;
		video::IVideoDriver *driver;
		gui::ICursorControl *cursor;
	};

	explicit Entering(Opts opts) : m_opts(opts) {}

	// Play chosen: the stages from the start
	void begin();
	// The world's on screen, or the game ended before it was: the page is
	// told there's nothing being entered, the planet let go
	void end();
	bool active() const { return m_active; }

	// A frame of the engine's load screen, at its progress (0..100)
	void step(int percent);

private:
	void stage(size_t index, std::optional<double> share);
	void frame(); // the planet, the page over it
	void tell();

	Opts m_opts;
	bool m_active = false;
	struct Seen
	{
		bool begun = false;
		double since = 0, until = 0; // seconds (porting::getTimeMs)
	};
	std::vector<Seen> m_seen;
	size_t m_at = 0;
	std::optional<double> m_share; // the running stage's, where it counts
	double m_last = 0; // the last frame (seconds)
	Json::Value m_told;
	double m_toldAt = 0;
};

} // namespace shell
