// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "world.h"
#include <json/json.h>
#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

/*
	A few blocks of the world, in memory: the world's own matter (WorldMatter)
	over a tiny ground of its own, from the game's blocks as baked
	(palette.json's `scene`). What happens in it is what would happen in the
	world, because it's the same code. No server, no map, no Lua.
*/
namespace sandbox {

// A block of the game's, as a scene needs it: its matter, and its shape and looks.
struct StageBlock
{
	WorldMatter::Spec spec;
	std::array<std::string, 3> tiles; // top, bottom, side: the game's tile strings
	std::vector<std::array<float, 6>> boxes; // a node box's parts (x0 y0 z0 x1 y1 z1, blocks from its corner); none: whole
	float friction = 0.6f; // what tyres get from it (0.6: ordinary)
	std::array<int, 4> pile{}; // if it's loose ground (pile::Material: repose, yield, bearing, give); bearing 0: it isn't
};

class Stage
{
public:
	struct Opts
	{
		const matter::Matter &matter;
		const std::vector<StageBlock> &blocks;
		int32_t x, y, z; // blocks along each side, 1..16
		double ambient;  // K
		double pressure; // Pa
		double gravity;  // m/s²
		std::vector<std::pair<std::string, double>> air; // by mole share
	};
	explicit Stage(const Opts &opts); // throws SandboxError on a bad size
	// The blocks as baked (palette.json's `scene`).
	static std::vector<StageBlock> read(const Json::Value &scene);

	// A block of the game's by name, as built. Before the first step. Throws on no such block.
	void place(Pos at, const std::string &node);
	void heat(Pos at, double temperature);
	// Kept at a temperature (something under it, a furnace's heat) until the scene's dropped.
	void hold(Pos at, double temperature);
	void add(Pos at, const std::string &substance, double moles, double temperature);
	// Poured from above something, moles a second till `until` (s): it heaps and runs.
	void pour(Pos at, const std::string &substance, double rate, double temperature, double until);
	// A scene as the wiki writes it: its blocks and its pours. Throws on what isn't one.
	void setUp(const Json::Value &spec, double ambient);
	void step(double seconds);

	struct Cell
	{
		int32_t block = -1;  // into the blocks given
		double level = 1;    // how full a levelled one is
		Liquid fill;         // the liquid in its open space
		uint32_t fill_colour = 0;
		double temperature = 0, activity = 0;
		int32_t main = -1;   // what fills it, by substance index
		matter::State state = matter::State::Solid;
		double filled = 0;   // its share
		int32_t vapour = -1;
		double haze = 0;
		uint32_t flame = 0;  // what's burning in it, 0xrrggbb; 0: nothing
		double boil = 0;     // its liquid: 0 well short of boiling, 1 boiling
	};
	std::vector<Cell> view() const; // x fastest, then z, then y (bottom layer first)
	double time() const { return m_time; }
	// What's pouring now: from where, and its substance's index.
	std::vector<std::pair<Pos, size_t>> pouring() const;
	double moles(const std::string &substance) const; // all of it in the stage, in anything
	int32_t sizeX() const { return m_x; }
	int32_t sizeY() const { return m_y; }
	int32_t sizeZ() const { return m_z; }

private:
	size_t index(Pos at) const;
	bool inside(Pos at) const;
	bool roomy(Pos at) const; // has open space for a liquid

	const matter::Matter &m_matter;
	const std::vector<StageBlock> &m_blocks;
	int32_t m_x, m_y, m_z;
	double m_ambient, m_pressure, m_time = 0;
	int32_t m_air = -1;
	std::vector<int32_t> m_kinds; // by cell
	std::vector<double> m_levels;
	std::vector<Liquid> m_fills;
	std::vector<uint32_t> m_colours;
	std::vector<int32_t> m_kindOf; // WorldMatter's kind → index into m_blocks
	std::map<size_t, std::pair<Pos, double>> m_holds;
	struct Blowing
	{
		Pos at;
		double rate, until; // moles of the room's air a second
	};
	std::vector<Blowing> m_blows;
	struct Pouring
	{
		Pos at;
		size_t substance;
		double rate, temperature, until;
	};
	std::vector<Pouring> m_pours;
	size_t substanceOf(const std::string &id) const;
	std::map<std::string, int32_t> m_byName;
	std::unique_ptr<WorldMatter> m_world;
};

} // namespace sandbox
