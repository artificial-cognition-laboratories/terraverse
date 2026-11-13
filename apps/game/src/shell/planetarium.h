// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <future>

#include "planetarium/planetarium.h"
#include <json/json.h>
#include <functional>
#include <memory>

namespace shell {

class Shell;

/*
	The planet a window shows under its page, if any (planning/sprint-33): the
	shell's side of the Planetarium (planetarium/: the renderer), a manager
	of the current one. One per window, as long as the page (the Shell owns
	it): the page can always ask. The page asks through the contract
	(ui/src/bridge/types.ts, `planetarium.*`), checked here where it
	arrives; whoever runs the window now (the game: its own world; the
	menu: a world code) says how a body is made from what the page asks
	(`serve`); with no one, nothing can be shown. Showing another body
	keeps the old one on screen until the new one's coarse layer is built,
	so the planet never blanks.

	It draws where the page leaves room for it (`viewport`, in page pixels;
	the whole window until it says).

	Back to the page: { kind: "planetarium", event: "picked", latitude,
	longitude, cell: { region, index, i, j, k } } for a click on the body;
	{ event: "hovered", body: { id, name, kind, x, y, radius } | null } as
	what's under the mouse changes or moves (page pixels; the star's id
	"star"); { event: "chosen", body: id } when a double click flies to one.
*/
class Planetarium
{
public:
	// The body the page asks for (`body` as the page sent it; may be null).
	// Throws shell::Refused for one this window can't show.
	// What it gives back is run on a worker: a body's atlas and system take long
	// enough to freeze the window (it may throw Refused there too; logged, the
	// view stays). The window keeps drawing what it has until it's made.
	using Making = std::function<planetarium::Planetarium::Opts()>;
	using Body = std::function<Making(const Json::Value &body)>;

	struct Opts
	{
		shell::Shell &shell;
	};

	explicit Planetarium(Opts opts); // registers the contract's verbs
	~Planetarium();                  // and takes them off

	// Who makes bodies from now (the menu, a game), hiding what was shown:
	// one window's planet doesn't follow into the next. Empty: no one. A
	// backdrop (a body as the page would ask for it) is shown now and
	// whenever the page hides the planet: the menu's universe, always there.
	void serve(Body body, std::optional<Json::Value> backdrop = std::nullopt);
	// The body shown stays on screen with no one serving, through a serve,
	// till it's let go: the menu's planet while its world is entered
	// (entering.h). Let go, it's hidden, and the latest server's from then.
	void hold();
	void release();

	bool shown() const { return m_current != nullptr || m_next != nullptr || m_wanted.has_value(); }
	// Shows a body directly (a key, a command line), as `planetarium.show` does.
	void show(planetarium::Planetarium::Opts body);
	// Hidden, the view's kept (parked) and shown again at once when the
	// same body's asked for next.
	void hide();
	// A body made ready while nothing's shown (a game's, as it joins): its
	// coarse land built in the background, so the first show is instant.
	void prepare(planetarium::Planetarium::Opts body);
	// Once a frame while hidden: a parked body's land goes on building, for
	// its camera kept over where the player is (latitude and longitude of
	// the ground, radians), so it opens at the detail it will be seen at.
	void idle(std::optional<std::pair<double, double>> player);
	// The player's orbit, drawn round its body (Planetarium::path); none: none.
	void path(std::optional<planetarium::Planetarium::Path> path);
	// The system's moment shown: a world's date (days into it, its year's
	// length: System::dayFor), the game's clock; or the host's own clock
	// (the menu's), run on by tick() while running, the page setting it.
	void dayIn(double worldDays, int32_t worldYear);
	void tick(double seconds);

	// The mouse where the page doesn't take it, while shown. Whether it was ours.
	bool handle(const SEvent &event);
	// Where the mouse is, anywhere on the window (pixels), a frame: the
	// menu's picture leans after it (the page takes the events themselves).
	void pointer(v2s32 at);
	// A frame, while shown: the planet over the whole screen, the scene
	// already begun; a click taken since is sent to the page.
	void draw(video::IVideoDriver *driver);

private:
	Json::Value show(const Json::Value &intent);
	Json::Value focus(const Json::Value &intent);
	Json::Value markers(const Json::Value &intent);
	Json::Value viewport(const Json::Value &intent);
	Json::Value fly(const Json::Value &intent);
	Json::Value frame(const Json::Value &intent);
	Json::Value layer(const Json::Value &intent);
	Json::Value time(const Json::Value &intent);
	Json::Value seams(const Json::Value &intent);
	// the body the page asked for, made, unless it's the one shown already
	void ask(const Json::Value &body);
	// a body's index in the system shown, by its id; Refused for none
	size_t bodyIndex(const planetarium::Planetarium &p, const Json::Value &id) const;

	Opts m_opts;
	Body m_body;
	bool m_holding = false; // hold(): the picture kept till release()
	void clear(); // what one server had shown, hidden, no one serving
	std::optional<Json::Value> m_backdrop;
	Json::Value m_asked; // the body shown (or being made), as asked for
	std::future<planetarium::Planetarium::Opts> m_making; // on the worker (one at a time: what they share isn't locked)
	bool m_makingWanted = false;   // what's on the worker is still what's wanted
	std::optional<Making> m_wanted; // the latest asked for, not yet started or being made
	std::vector<std::function<void()>> m_afterMade; // what the page asked of a body not made yet
	void start();                  // the wanted body onto the worker, if it's free
	void collect();                // one the worker's finished, onto the screen
	bool waiting() const { return m_wanted.has_value(); }
	std::unique_ptr<planetarium::Planetarium> m_current; // on screen
	std::unique_ptr<planetarium::Planetarium> m_next;    // being made ready to take its place
	std::unique_ptr<planetarium::Planetarium> m_parked;  // hidden, kept to show again
	std::unique_ptr<planetarium::Planetarium> m_retiring; // replaced, kept till its successor's drawn once
	bool m_looked = false; // the parked one has been on screen: its camera's the player's now
	std::optional<std::pair<double, double>> m_over; // where the parked camera's over (idle)
	v2u32 m_screen{ 1280, 720 }; // the window, as last drawn (a parked body builds for it)
	std::vector<planetarium::MarkerLayer::Marker> m_markers; // the page's, kept across a swap
	// where the page leaves room for it (page pixels); none: the whole window
	struct Rect
	{
		double x, y, width, height;
	};
	std::optional<Rect> m_view;
	// what the page covers round the view's edges (page pixels): the planet
	// is framed in the rest
	struct Insets
	{
		double left = 0, right = 0, top = 0, bottom = 0;
	};
	Insets m_insets;
	std::optional<planetarium::Planetarium::Layer> m_layer; // the page's, kept across a swap
	bool m_seams = false;
	// the host's clock (the menu's): the playable body's year through, running or not
	struct Clock
	{
		double date = 0.25;  // 0..1 of the playable body's orbit
		bool running = false; // the menu's backdrop holds still; the create viewer runs it
		double rate = 1.0 / (365.25 * 60); // of an orbit a second: a day a minute (Earth's)
		double reported = 0; // seconds since the page was last told
	};
	Clock m_clock;
	// the world being made for the page (its code, as its steps are
	// recorded: terragen/progress.h), since when (terragen::clock()), and
	// what the page was last told of it (null: nothing being made), when
	std::string m_makingCode;
	double m_makingSince = 0;
	Json::Value m_told;
	double m_toldAt = 0;
	void tellMaking();
	// The page's composed view (planetarium.scene): which shot, if any, eased
	// to from the framing it was at, a framing at a time (so any shot to any
	// other is one smooth move); none: the camera's the page's and the mouse's.
	enum class Shot { Home, Play };
	struct Scene
	{
		std::optional<Shot> shot;
		std::optional<planetarium::Framing> from, now; // eased from; as last composed
		double t = 0, seconds = 1;
		double home = 0; // how far it's at the home shot, 0..1: the lean's share
	};
	// Play's camera taken by the mouse (dragged, wheeled): the view's own orbit
	// controls from where the shot left it, no longer composed. The next shot
	// eases from wherever it was left (Planetarium::framed).
	bool m_held = false;
	v2f m_heldCentre; // the shot's own centre when it was taken (mid-move: not yet the room's middle)
	v2f room() const; // the middle of what the page leaves, from the window's (its half widths and heights)
	Scene m_scene;
	planetarium::Framing framingFor(Shot shot) const; // as it'd be now, on this screen
	// The composed picture alive: the camera orbits a little after the mouse,
	// eased (the planet still on screen, the sky passing behind, so the stars
	// catch); with no mouse it wanders on its own; the planet turns.
	struct Lean
	{
		v2f want{ 0, 0 }, now{ 0, 0 }; // −1..1 of the window, +y up
		double since = 0;              // seconds, for the wander
	};
	Lean m_lean;
	planetarium::Shot leaned() const; // HOME_SHOT, as the lean has it now (its share of it)
	bool m_orbits = true; // the page's (planetarium.orbits)
	Json::Value scene(const Json::Value &intent);
public:
	// The picture already there: no easing in (a menu opening on it).
	void opening();
private:
	void compose(planetarium::Planetarium &p); // to the scene's framing now
	void apply(planetarium::Planetarium &p) const; // the clock and the page's view, onto a body
	// what the page was last told is under the mouse; what's there now, told if it's changed
	std::optional<planetarium::Planetarium::Hover> m_hovered;
	void tellHover(const planetarium::Planetarium &p);
};

} // namespace shell
