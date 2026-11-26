// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irr_aabb3d.h"
#include "src/shared/base/irr_v3d.h"
#include <line3d.h>
#include <json/json.h>
#include "library/geometry/shape/shape.h"
#include "src/shared/game/dig/earthwork.h"
#include <array>
#include <map>
#include <optional>
#include <unordered_map>
#include <string>
#include <utility>
#include <vector>

class Client;
struct PointedThing;
namespace shell { class Ui; }

/*
	Digging by hand, as the client sees it (sprint 124; the server's is
	server/game/dig): holding a shovel, F is its forms' menu (a hole, a
	trench, a pit, a flattened pad, each in sizes), wherever they look. The
	form is outlined whole where they aim, snapped as a member is
	(earthwork::place); held, each swing asks the server for a bite of it,
	the box kept where it was when the button went down, so it digs out
	what was outlined. Nothing's predicted. Read from the UI value "dig".
*/
namespace dig {

class Digging
{
public:
	struct Opts
	{
		Client *client;
		const shell::Ui &ui;
	};
	explicit Digging(const Opts &opts) : m_opts(opts) {}

	// A shovel held: F is its intents' menu.
	bool menu() const;
	// A dig holding a shovel, on ground it moves: asked of the server where they aim. False if not.
	// Each frame: how a form goes where they aim (Ctrl: freely; Shift: a level at the ground
	// aimed at), and whether the dig button's down (while it is, the form stays put).
	enum class Button { Up, Down };
	void look(earthwork::Mode mode, Button button);
	// The same, and where they look (a bore's along it).
	void look(earthwork::Mode mode, Button button, const core::line3d<f32> &ray);
	// A swing holding a shovel at ground: a bite of the form asked of the server. False if not.
	bool take(const PointedThing &pointed);
	// The form where it would be dug (BS, about the pointed node's centre): empty where it wouldn't.
	std::vector<aabb3f> preview(const PointedThing &pointed);
	// A bore's outline, if the chosen operation bores: a circle of the face's sixteenths round where
	// they aim, outlined on them, and the
	// next cells it'll take, the nearest first (BS, about `node`'s centre).
	struct Bore
	{
		v3s16 node;
		std::vector<std::pair<v3f, v3f>> ring;
		std::vector<aabb3f> next;
	};
	std::optional<Bore> bore(const PointedThing &pointed);
	// Each frame, after bore(): held, the next cell asked of the server at the tool's rate, one at
	// a time, evenly.
	void swing(f32 dtime);

private:
	// Whether what's held is a shovel; and that, pointed at ground it moves.
	bool holds() const;
	bool holds(const PointedThing &pointed) const;
	// The form aimed at, kept while the button's down: none where nothing would be dug.
	std::optional<earthwork::Target> target(const PointedThing &pointed);
	// Whether the chosen operation bores along where they look.
	bool bores() const;
	// The held tool, as the server told it: none till it has.
	struct Tool
	{
		s32 cell; // sixteenths: the side of the cells it bores
		f32 rate; // cells a second it bores
	};
	std::optional<Tool> tool() const;
	// What's filled in a box of world sixteenths (inclusive), read a block at a time: a bore's ring
	// asks it of tens of thousands of sixteenths a frame.
	struct Grid
	{
		v3s32 lo, size;
		std::vector<bool> filled;
		size_t index(v3s32 c) const { return ((size_t)(c.Y - lo.Y) * size.Z + (c.Z - lo.Z)) * size.X + (c.X - lo.X); }
		// outside the box: not filled
		bool at(v3s32 c) const
		{
			const v3s32 d = c - lo;
			return d.X >= 0 && d.Y >= 0 && d.Z >= 0 && d.X < size.X && d.Y < size.Y && d.Z < size.Z && filled[index(c)];
		}
	};
	Grid fill(v3s32 lo, v3s32 hi) const;
	struct EdgeHash
	{
		size_t operator()(const std::array<s32, 6> &e) const
		{
			size_t h = 0;
			for (s32 v : e)
				h = h * 1000003u ^ (size_t)(u32)v;
			return h;
		}
	};
	// Whether the world sixteenth c is filled (m_shapes).
	bool solid(v3s32 c) const;
	static constexpr int NEXT = 4;   // cells lit ahead of a bore
	static constexpr int SLOPE = 24; // sixteenths: how far over or under the aimed face a ring's face is looked for
	// The chosen operation's size, and its operation's shape (the first's, none chosen).
	std::pair<Json::Value, std::string> chosen() const;
	// The ground's top over a column near `near` (world sixteenths), or `near` if there's none.
	// Each block's shape read once a frame (m_shapes): a footprint's columns share a few blocks,
	// and reading smoothed ground is dear (thousands a frame froze the game).
	s32 surface(s32 x, s32 z, s32 near) const;
	// Where they aim, a world sixteenth.
	static v3s32 aimOf(const PointedThing &pointed);

	Opts m_opts;
	earthwork::Mode m_mode = earthwork::Mode::Snap;
	Button m_button = Button::Up;
	v3f m_dir{ 0, 0, 1 }; // where they look
	v3f m_eye;            // from where, world sixteenths
	std::vector<v3s32> m_next;            // a bore's next cells, as lit
	std::map<v3s32, f32> m_sent;          // cells asked for → seconds till they're forgotten
	f32 m_owed = 0;                       // a cell's due when it's one
	static constexpr f32 SENT = 0.5f;     // seconds an asked-for cell's taken as gone
	std::optional<earthwork::Target> m_kept; // the form being dug, while the button's down
	// blocks' solid shapes (none: whole; an absent entry: open or not loaded), this frame
	mutable std::unordered_map<v3s16, std::optional<std::optional<shape::Shape>>> m_shapes;
};

} // namespace dig
