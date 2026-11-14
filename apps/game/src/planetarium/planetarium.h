// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "far/palette.h"
#include "far/terrain.h"
#include "far/tiles.h"
#include "planet/atlas.h"
#include "planetarium/camera.h"
#include "planetarium/globe.h"
#include "planetarium/markers.h"
#include "planetarium/paint.h"
#include "planetarium/seams.h"
#include "planetarium/shot.h"
#include "planetarium/system.h"
#include <chrono>
#include <future>
#include <unordered_set>
#include <map>
#include <set>
#include <memory>

struct SEvent;
class IWritableShaderSource;
namespace video { class IVideoDriver; }

/*
	The Planetarium (planning/sprint-31-planetarium.md): one planet viewer,
	native, drawn under whatever page is shown over it. A body's land is
	far::Tiles for the camera's eye, the same tiles the far view draws in
	game; layers draw over it (the atlas's seams, for now). Hosts make one
	and give it the window: the game (a key), and `luanti --planetarium`
	(a world code alone, from the game's baked palette).
*/
namespace planetarium {

// The system's index of an atlas body (planet::Atlas::HOME: the playable
// one; another's found from the world code, terragen/worlds.h).
size_t systemIndex(const planet::Atlas &atlas, const far::Horizon &horizon, const terragen::System &system, int body);

class Planetarium
{
public:
	struct Opts
	{
		planet::Atlas atlas;
		far::Horizon horizon;
		far::Palette palette;
		// the system the body is in
		std::shared_ptr<const terragen::System> system;
		// the atlas's body drawn from its tiles (the home: the system's
		// playable body); the rest of the system round it
		int body = planet::Atlas::HOME;
	};

	explicit Planetarium(Opts opts);
	~Planetarium();

	// The mouse: drag to orbit, the wheel to go up and down; a click (a press
	// let go where it was) is kept for takeClick; a double click on a body
	// flies to it (the star: out to the whole system). Only what starts in
	// the last frame's view (a drag may leave it). Whether it was the
	// Planetarium's.
	bool handle(const SEvent &event);

	// A place on the body: where, and the atlas cell at the sea there.
	struct Pick
	{
		double latitude, longitude; // radians
		terragen::Atlas::Cell cell;
	};
	// The last click, once: the place under it, or none off the body.
	std::optional<Pick> takeClick();
	// The place under a window pixel of the last frame's view; none off the body.
	std::optional<Pick> pick(double x, double y) const;

	// The body under the mouse in the last frame (not while dragging): its
	// index (none: the star), where it's drawn and how big (window pixels).
	// The body the camera's round, filling the screen, isn't hovered.
	struct Hover
	{
		std::optional<size_t> body;
		v2f at;
		float radius;
	};
	const std::optional<Hover> &hovered() const { return m_hover; }
	// A body a double click flew to, once (none: the star).
	std::optional<std::optional<size_t>> takeChosen();

	// Whether the whole body is drawn, if coarsely (its coarse tiles built).
	bool ready() const { return m_tiles.based(); }
	// Its land up to the GPU before its first frame, `ms` of it a call: drawn
	// all at once, a whole planet's meshes are a hitch. Whether it's all there.
	bool warm(video::IVideoDriver *driver, double ms);
	// How far it is to ready() (0..1): what a page shows while a body's made.
	float readiness() const { return m_tiles.basedShare(); }
	// Whether all the camera's own tiles are built (the detail it wants, not just the coarse whole).
	bool settled() const { return m_tiles.based() && m_tiles.pending() == 0; }
	// Its globe's made too (what's drawn from far out: planetarium/globe.h)
	bool globed() const { return m_globe && m_globe->ready(); }
	// What the tiled body's land costs to draw now: tiles, draw calls (their
	// buffers), vertices (the ladder reports them: --planetarium-ladder)
	struct DrawStats
	{
		size_t tiles = 0, buffers = 0, vertices = 0;
	};
	DrawStats drawStats() const;

	// Over a place on the tiled body (Opts::body) (its ground's latitude and longitude):
	// the camera goes round to it as the body is turned now.
	void focus(double latitude, double longitude, std::optional<double> altitude);
	// A flight: to a body (its index), over a place on it (latitude and
	// longitude of its ground, radians) and at an altitude (blocks), each
	// kept as it is where not given; eased over `seconds` (0: there now).
	// From afar it climbs to see both bodies, and comes down.
	struct Flight
	{
		size_t body;
		std::optional<double> latitude, longitude, altitude;
		std::optional<double> radii; // or the altitude in the body's radii
		double seconds;
		std::optional<OrbitCamera> camera = std::nullopt; // or exactly this one (round `body`)
	};
	void fly(const Flight &flight);
	// Over the ground the star stands above now, a little past noon (radians of longitude).
	double litLongitude() const;
	// To a composed view of the tiled body, as the day stands (day first). The
	// framing's centre is the host's to aim (aim()).
	void compose(const Framing &framing, double seconds);
	// A shot as its framing on a screen this shape (the star's place on it
	// as the way to the star)
	static Framing framingOf(const Shot &shot, float aspect);
	// The framing the camera's at now (where a flight ends, mid-flight): compose's
	// inverse, so a move to a shot can start from where the mouse left it.
	Framing framed() const;
	// Whether the bodies' orbits are drawn.
	void orbits(bool shown) { m_system.orbits(shown); }
	bool flying() const { return m_flight.has_value(); }

	// Where the view's middle is drawn (the free part of the screen: as
	// OrbitCamera::perspective's shift).
	void frame(v2f shift) { m_shift = shift; }
	// Where the body's centre is drawn, by sliding the lens with the frame's
	// shift: a composed shot's place for it (Shot::centre).
	void aim(v2f at) { m_aim = at; }
	// How bright the galaxy behind it is drawn, 0..1.
	void skyLevel(float level) { m_system.skyLevel(level); }

	// An overlay on the land (planning/sprint-83-planetarium.md): painted
	// from the world model's fields as each tile is built (Painter), the
	// land's relief kept as shading; on every body drawn from tiles, but
	// only the tiled body's when it's masked by what's been explored. None:
	// the land as it is.
	struct Layer
	{
		Overlay overlay;
		std::shared_ptr<const Painter::Opts::Image> image; // instead (Painter::Opts)
		std::optional<double> date; // 0..1 through the year; none: the year's mean
		std::optional<std::vector<u8>> explored; // Painter::Opts
		u32 grid = 0;
	};
	void layer(std::optional<Layer> layer);
	// Whether the atlas's seams (where its regions meet) are drawn.
	void seams(bool shown) { m_seamsShown = shown; }
	// A body shown cut open (its index; none: none): its shells, from its card.
	void cutaway(std::optional<size_t> body) { m_system.cutaway(body); }

	// The same view as another's (a body swapped in for it): its camera, its
	// moment, the body it's round.
	void keep(const Planetarium &was);
	// The camera as `was` has it, the same way in space (so the sky behind
	// stays put), round this body at as many of its radii: its own ground
	// and time of day under it. After day(): this body's spin is the turn.
	void align(const Planetarium &was);
	// How high the camera is, in the body's radii.
	double radiiUp() const { return (m_flight ? m_flight->end : m_camera).altitude() / m_sea; }
	v2f shift() const { return m_shift; }
	// Goes to another body of the system (its index there): the camera in
	// orbit round it, at its scale. The tiled body's the one picked on.
	// (The star is a body too, to fly to and go round: index star().)
	void focusBody(size_t body);
	size_t star() const { return m_system.system().spec().bodies.size(); }
	size_t target() const { return m_target; }
	int body() const { return m_body; } // the atlas's body it's made for (Opts::body)
	const std::string &code() const { return m_horizon.code; }
	// the body it is round, or a flight is taking it to
	size_t heading() const { return m_flight ? m_flight->to : m_target; }
	const terragen::System &system() const { return m_system.system(); }
	size_t tiled() const { return m_tiled; }
	void markers(std::vector<MarkerLayer::Marker> markers) { m_markers.set(std::move(markers)); }
	// A path round an atlas body (its index; points in its frame, blocks
	// from its centre: the player's orbit), drawn while that's the tiled
	// body; none: none.
	struct Path
	{
		int body;
		std::vector<OrbitCamera::Vec> points;
	};
	void path(std::optional<Path> path) { m_path = std::move(path); }
	// The system's moment shown: days after its day 0. The tiled body's
	// spin: its time of day (0..1, noon at ½ on longitude 0, as the game's
	// sun has it), or where its own sidereal day has turned it by then.
	void day(double days, std::optional<double> timeOfDay = std::nullopt);

	// Once a frame, drawn or not: the tiles for the camera on a screen this
	// size, built and taken onto the GPU. Throws if a tile couldn't be built.
	void update(v2u32 screen);
	// A frame into `view` (window pixels: the whole window, or the part a
	// page leaves for it); the scene already begun. Updates first.
	void draw(video::IVideoDriver *driver, core::recti view);

	const OrbitCamera &camera() const { return m_camera; }

private:
	// made at the first frame: there's a window by then
	std::unique_ptr<IWritableShaderSource> m_shaders;
	const far::Palette m_palette; // before the tiles that read it
	const terragen::Atlas m_atlas;
	const double m_sea;
	far::Tiles m_tiles;
	// the tiled body from afar, once its tiles' cells are about a pixel; made
	// once its coarse tiles are there (not to slow the first view's)
	std::unique_ptr<Globe> m_globe;
	bool m_globed = false; // it's drawn as its globe now (with some give, so it doesn't flicker on the line)
	OrbitCamera m_camera;
	SeamLayer m_seams;
	bool m_seamsShown = false;
	MarkerLayer m_markers;
	SystemLayer m_system;
	const int m_body; // the atlas's body drawn from its tiles (Opts::body)
	std::optional<Path> m_path;
	void drawPath(video::IVideoDriver *driver, const OrbitCamera::Vec &ground);
	size_t m_tiled;  // the system's body drawn from its tiles (Opts::body)
	size_t m_target; // the body the camera's round
	double m_day = 0;
	double m_spin = 0; // radians: the tiled body turned, its ground to space
	float m_fov = OrbitCamera::FOV;
	v2f m_shift;       // where the view's middle is drawn (frame)
	v2f m_aim;         // where the body's centre is drawn (aim)
	std::unordered_set<const scene::SMeshBuffer *> m_warmed; // (warm)
	v2f lens() const; // the projection's shift: the frame's, and the aim's
	std::optional<Layer> m_layer;
	// the land's colours under a layer, from that body's world model (an atlas body's)
	// (none while its world's still being built: then it's tinted when it is)
	std::shared_ptr<const far::Tiles::Tint> tintFor(int body);
	float contour() const;
	void tint(int body); // that body's tiles, as the layer has them
	// an atlas body's world model, once built (on another thread: seconds, the first time)
	std::shared_ptr<const terragen::World> worldOf(int body);
	std::map<int, std::shared_future<std::shared_ptr<const terragen::World>>> m_worlds;
	std::set<int> m_untinted; // bodies whose tint waits for their world
	// The rest of the atlas's bodies, drawn from their own tiles once big
	// enough on screen, for an eye as far off as the camera is: their
	// coarsest land from afar, finer on the way in.
	struct Baked
	{
		int body;     // the atlas's
		size_t index; // the system's
		double sea;   // its sea's radius (its own blocks)
		std::unique_ptr<far::Tiles> tiles; // made when first drawn
	};
	std::vector<Baked> m_baked;
	const planet::Atlas m_world;
	const far::Horizon m_horizon;
	// a baked body in its place (SystemLayer::Surface); false: not one, or not built yet
	bool drawBaked(video::IVideoDriver *driver, const core::matrix4 &look, float aspect, size_t index,
			const terragen::System::Vec &at, double pixels, float near, float far);
	double spinOf(size_t index) const; // a body's turn on its axis now (radians), by its own day
	// where the camera is from the tiled body's centre (blocks), and how it looks
	OrbitCamera::Vec eye() const;
	core::matrix4 view() const;
	// a camera round a body, at its scale
	OrbitCamera cameraFor(size_t body) const;
	// where a camera round a body is from its centre, in the scene
	OrbitCamera::Vec offsetOf(size_t body, const OrbitCamera &camera) const;
	// a flight under way: from where the camera was (from its body's centre),
	// to a camera round another body
	struct Flying
	{
		size_t from;
		OrbitCamera::Vec start; // from `from`'s centre, blocks
		size_t to;
		OrbitCamera end;
		double seconds, t = 0;
		OrbitCamera::Vec up; // the camera's up as it set off: turned to the end's on the way
	};
	OrbitCamera::Vec endUp() const; // a flight's camera's up where it lands (the scene's)
	std::optional<Flying> m_flight;
	struct Pose
	{
		OrbitCamera::Vec at, look; // the scene's (blocks)
	};
	Pose pose() const; // the camera now, flying or not
	OrbitCamera::Vec centreOf(size_t body) const; // in the scene
	double radiusOf(size_t body) const;           // blocks (the tiled body: its sea)
	std::chrono::steady_clock::time_point m_last; // the last update, for flights
	bool m_dragging = false;
	s32 m_x = 0, m_y = 0;          // the mouse, while dragging
	s32 m_downX = 0, m_downY = 0;  // where the press was
	std::optional<v2s32> m_click;  // a click not yet taken
	std::optional<v2s32> m_mouse;  // over the view, not dragging
	std::optional<Hover> m_hover;  // under it, the last frame
	std::optional<std::optional<size_t>> m_chosen; // a double click's, not yet taken
	// what's under the mouse, the camera at `eye` looking by `look`
	std::optional<Hover> hoverFor(const OrbitCamera::Vec &eye, const core::matrix4 &look, float aspect) const;
	v2u32 m_screen{ 1, 1 };        // the last frame's view: its size,
	v2s32 m_corner{ 0, 0 };        // and where it is in the window
	u32 m_frame = 0;
	double m_worstFrame = 0, m_worstTiles = 0; // ms, since the last log
};

} // namespace planetarium
