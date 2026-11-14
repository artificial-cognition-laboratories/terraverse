// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "physics.h"
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/*
	The world as a sandbox: its own blocks run the shared physics (physics.h)
	wherever something is happening — a fire in a hole, a clay kiln, lava
	meeting water — and sleep everywhere else.

	- Awake blocks hold their matter and temperature and run reactions,
	  conduction and gas exchange with their neighbours.
	- An asleep block is what its kind of block is made of, at the weather's
	  temperature. Heat flowing into a solid one wakes it once it has warmed
	  enough to matter; asleep air is the room (oxygen in, smoke out).
	- When an awake block's matter changes enough, the block changes: to the
	  kind registered as the form of what now fills it (wood → charcoal →
	  air, wet clay → fired clay, water → ice).
	- A settled block (at the weather's temperature, nothing reacting, its
	  matter what its kind is made of) goes back to sleep.

	The world itself is reached through `Ground`, so this has no engine
	dependencies (and tests run it over a map of their own).
*/
namespace sandbox {

struct Pos
{
	int32_t x, y, z;
};

// A block's open space (sprint 81), from the bottom up: how many of its
// sixteenths are empty in each of its 16 layers (256 each: wholly open; 0:
// solid). A sculpted block is partly open; liquid lies in what's empty.
struct Room
{
	uint16_t layers[16];
	static Room open();
	static Room closed();
	uint32_t total() const;
	// Where a liquid's surface stands (0..1 of the block) holding `share` of its open space.
	double surface(double share) const;
	// The lowest layer with any opening, 16 where there's none.
	int lowest() const;
};

// The liquid in a block's open space, as the world keeps it asleep (sprint 81).
struct Liquid
{
	int32_t substance = -1; // −1: none
	double share = 0;       // of its open space
};

class WorldMatter
{
public:
	// The world, as this sees it.
	struct Ground
	{
		std::function<int32_t(Pos)> kind;         // the registered kind at a position, −1 if none (it exchanges nothing)
		// make the block at a position this kind; a levelled kind's level (0..1] goes with it
		std::function<void(Pos, int32_t, double)> become;
		std::function<double(Pos)> ambient;       // K: the weather's temperature there
		// how full a levelled block is, 0..1 (sprint 80: a puddle, a heap); 1 for any other
		std::function<double(Pos)> level;
		// sprint 81: a block's open space, and which of its layers are open
		// through a side (0..3: +x −x +z −z), a bit each, from the bottom
		std::function<Room(Pos)> room;
		std::function<uint16_t(Pos, int)> face;
		// the liquid in its open space, kept by the world (a fill), and its
		// keeping: a substance (−1: none), a share, a colour (0xRRGGBB)
		std::function<Liquid(Pos)> fill;
		std::function<void(Pos, int32_t, double, uint32_t)> setFill;
		// which of the airs (Opts::airs) a position is in: each body its own
		// (sprint 96: fire fails on the Moon); unset with only one
		std::function<size_t(Pos)> air;
		// a block melting from outside in (sprint 104): cut it to so many of its
		// 4096 sixteenths, its open faces a bit each (shape::melted). Unset: it
		// keeps its water in till it's all melted.
		std::function<void(Pos, int, unsigned, bool)> carve;
		// how wet the ground is at a position, 0 (dry) .. 1 (as wet as it
		// gets), from the weather (sprint 111): a porous block's pores hold
		// that share of what they can. Unset: dry
		std::function<double(Pos)> wetness;
		// whether a block of liquid is a river's or a lake's, kept full by the
		// network it's part of (sprint 115: fill::HELD): what runs out of it is
		// made up, as a reach's discharge would. Unset: none is
		std::function<bool(Pos)> held;
	};
	// The air of a world's body: its pressure at the ground and its gases.
	struct Air
	{
		double pressure;                                   // Pa (0: none)
		double gravity;                                    // m/s²: what pulls liquids down and along
		std::vector<std::pair<std::string, double>> gases; // by mole share (none: no air)
	};
	struct Opts
	{
		const matter::Matter &matter;
		std::vector<Air> airs; // the first is the one a kind is made in (its properties)
		Ground ground;
	};
	explicit WorldMatter(const Opts &opts);

	// A kind of block: what a node is made of.
	struct Spec
	{
		std::string node;
		std::string matter;    // a substance or rock id from the matter tables, or "air"
		// or a mix: substances and rocks, each with its share of the solid part
		// (a soil point is quartz, kaolinite and organic matter; a rock blend two rocks)
		std::vector<std::pair<std::string, double>> parts;
		double share = 1;      // of its volume (the rest is air in its pores: a log is ~40% wood)
		matter::State state = matter::State::Solid; // ice is water, solid
		bool form = false;     // the block that matter becomes when it fills one
		bool flowing = false;  // a liquid the engine flows itself (water, lava): not poured here
		// a vessel (sprint 79: a crucible): air that holds what's put in it,
		// staying the vessel whatever fills it; asleep only when it's empty
		bool vessel = false;
		// sprint 80: the block's param2 says how full it is (a puddle, a heap
		// of powder, the sea's full blocks are 1); its matter is that share
		bool leveled = false;
		// a powder: what fills it piles at its angle of repose and pours (a
		// heap of salt), where a block of it stands as built
		bool loose = false;
		// sprint 111: how much of its pores (the 1 − share not its matter)
		// water fills where the ground's as wet as it gets: soil, by its
		// texture (clay holds most, sand least). 0: its pores are air.
		double holds = 0;
	};
	int32_t kind(const Spec &spec); // returns its id
	// A kind's mass per cubic metre (a block's) at room temperature: what
	// it weighs as a thing that moves (sprint 57). Throws on no such kind.
	double density(int32_t kind) const;
	const std::string &node(int32_t kind) const;

	// Put heat into a block (a burner, of so many watts): wakes it.
	void warm(Pos at, double joules);
	// Bring a block to at least a temperature (a spark, a torch): wakes it.
	void heat(Pos at, double temperature);
	// Set a block's temperature exactly (a heat gun, a quench): wakes it.
	void touch(Pos at, double temperature);
	// Push the room's air into a block (a bellows): moles now; wakes it.
	void blow(Pos at, double moles);
	// Put matter into a block (sprint 79: a torch's gas, a cup poured):
	// moles of a substance at a temperature; wakes it.
	void add(Pos at, size_t substance, double moles, double temperature);
	// The same as a powder or a pour (sprint 80): what lands piles and flows.
	void pour(Pos at, size_t substance, double moles, double temperature);
	// Something changed at a position (dug, built): the liquids and powders
	// there and beside it wake, to run into the hole or pile against the wall.
	void disturb(Pos at);
	// Step the awake blocks by dt seconds. A block whose kind has changed
	// underneath (dug, built over) is dropped first: the world wins.
	void step(double dt);

	double temperature(Pos at) const; // awake: its own; asleep: the weather's
	double capacity(Pos at) const;    // J/K: the heat its matter holds per kelvin (−1: no matter)
	struct Awake
	{
		Pos pos;
		double temperature, activity; // K, W of reaction heat (+ given off)
		int32_t main;                 // substance filling most of it, −1: only gas
		matter::State state;
		double fill;
		int32_t vapour;               // the gas most unlike the room's air, −1: none
		double haze;
		uint32_t flame = 0;           // the colour of what's burning fastest in it, 0: no flame
	};
	std::vector<Awake> awake(Pos min, Pos max) const;
	// One block, awake or asleep (asleep: what its kind is, at the weather's
	// temperature); nullopt where there's no matter registered.
	std::optional<Awake> inspect(Pos at) const;
	// What's in a block (sprint 79), awake or as its kind is made; nullopt
	// where there's no matter registered.
	std::optional<matter::Mixture> mixture(Pos at) const;
	size_t awakeCount() const { return m_cells.size(); }
	size_t heldCount() const { return m_held.size(); } // asleep blocks keeping a mixture of their own
	// The most blocks stepped in one go (sprint 80): past it, the most active
	// go first and the rest wait their turn, their chemistry's time owed.
	void budget(size_t most) { m_budget = most; }
	// Reactions that ran noticeably since the last call, and where (once each per block).
	std::vector<std::pair<Pos, size_t>> happened();

	std::string save() const;
	void load(const std::string &bytes); // throws SandboxError if it isn't this table's

private:
	struct Kind
	{
		std::string node;
		std::vector<std::pair<size_t, double>> by_volume; // solids at room temperature
		bool air, form, flowing, vessel, leveled, loose;
		int32_t main;       // its main substance, for forms (−1 for air)
		matter::State state;
		double fill;
		double k, capacity; // asleep at the weather's temperature: W/(m·K), J/K
		double pores = 0;   // share of its volume water fills where the ground's wettest (sprint 111)
	};
	struct Cell
	{
		Pos pos;
		int32_t kind;
		matter::Mixture mix;
		uint32_t sky = 0;    // which air it's in (sprint 96)
		double quiet = 0;    // s settled
		double activity = 0; // W
		uint32_t flame = 0;  // this step's, as Awake's
		// this step's (sprint 79: worked out once, not per neighbour): W/(m·K),
		// J/K, and whether it's mostly solid or liquid
		double k = 0, capacity = 0;
		bool dense = false;
		// sprint 80: flowing. What's poured or a powder is loose (it piles
		// and slides); a liquid's outflow through each side (+x −x +z −z), m³/s,
		// kept between steps (Mei's virtual pipes: it has momentum)
		bool loose = false;
		bool held = false; // a river's or lake's water, kept full (Ground::held)
		double flux[4] = { 0, 0, 0, 0 };
		double liquid = 0, solid = 0; // m³, worked out once a flow step
		double open = 0;              // m³ of room for liquid and what's poured (its open space)
		Room room = Room::open();     // its open space by layer (sprint 81)
		double room_at = -1;          // when it was looked at (the world's clock)
		double shown = -1;            // the level last drawn
		double drawn_at = -1e9;       // s (the world's clock): when
		double filled = -1, filled_at = -1e9; // its fill last kept (sprint 81), and when
		uint32_t fill_colour = 0;
		double owed = 0;              // s of chemistry not yet run (the budget passed it over)
		double moved = 0;             // m³ flowed in or out last step: how busy it is
		int carved = 4096;            // the sixteenths of it left, melting
		double carved_at = -1e9;      // s (the world's clock): when
	};
	// An asleep block whose matter isn't simply its kind's (a puddle of brine,
	// a heap a little over its drawn level, a crucible of bronze): kept as it was.
	struct Held
	{
		Pos pos;
		int32_t kind;
		matter::Mixture mix;
		bool loose;
	};

	static constexpr double FULL = 63.5 / 64; // fuller than this, a liquid is its plain block (sprint 80)
	static int64_t key(Pos p);
	static bool same(const Kind &a, const Kind &b); // the same matter in the same state
	Cell *wake(Pos at, double temperature);
	// The air a block lies in, as the physics runs it: its pressure, and its gas by substance.
	struct Sky
	{
		Physics physics;
		std::vector<double> air;
		double gravity;
		bool vacuum; // no air: gas a block sheds expands away (it wakes nothing)
	};
	matter::Mixture made(const Kind &k, const Sky &sky, double temperature, double level = 1) const;
	matter::Mixture asleep(Pos at, const Kind &k, double temperature) const;
	void showFill(Cell &c, bool exactly); // its liquid kept as the ground's fill (sprint 81)
	void sleep(std::unordered_map<int64_t, Cell>::iterator it);
	double wakeAt(const Kind &k, const Sky &sky, double ambient) const; // where an asleep block of a kind really is
	void exchange(Cell &c, double k, double dt);
	void reshape(Cell &c, bool exactly = false);
	bool melting(Cell &c, bool exactly); // a block part melted: cut to what's left of it
	int32_t fullForm(int32_t substance, matter::State state) const;
	int32_t airForm() const; // the air a liquid lies in (sprint 81) // a liquid's plain (unlevelled) block, −1: none
	void flow(const std::vector<int64_t> &ids, double dt); // liquids and powders: sprint 80 (flow.cpp)
	void evaporate(Cell &c, double dt);
	double surface(Pos p, int depth = 0) const; // the height a liquid's surface stands at there, or NAN: a wall
	void volumes(Cell &c) const;
	double taken(const Cell &c) const; // m³ of its open space taken (sprint 81)
	double density(const Cell &c, matter::State state) const; // kg/m³ of what's in it in that state
	bool settled(const Cell &c) const;
	size_t skyAt(Pos at) const;
	// the first air's: for what doesn't hang on the air (volumes, conduction, light)
	const Physics &base() const { return m_skies[0].physics; }
	const Sky &sky(Pos at) const { return m_skies[skyAt(at)]; }
	const Sky &sky(const Cell &c) const { return m_skies[c.sky]; }
	double unlike(const matter::Mixture &b, const Sky &sky) const; // how far its gas is from the room's air (the largest excess share)

	const matter::Matter &m_matter;
	int32_t m_water = -1; // the substance pores hold (sprint 111), −1 in tables without it
	Ground m_ground;
	std::vector<Sky> m_skies;
	std::vector<Kind> m_kinds;
	std::unordered_map<std::string, int32_t> m_by_node;
	std::unordered_map<int64_t, Cell> m_cells;
	std::unordered_map<int64_t, Held> m_held;
	size_t m_budget = 8192;
	double m_clock = 0; // s stepped
	std::unordered_map<int64_t, double> m_pending; // J flowed into asleep solid blocks, not yet enough to wake them
	std::vector<std::pair<Pos, size_t>> m_happened;
	std::unordered_map<int64_t, std::vector<bool>> m_seen; // per block, reactions already reported
};

} // namespace sandbox
