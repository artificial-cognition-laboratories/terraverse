// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "client/liquidlook.h"
#include "sandbox/stage.h"
#include <array>
#include <vector>

namespace shell {

/*
	A scene's liquid as the game draws it near the camera: columns a sixteenth
	wide, running as shallow water (client/shallow.h) with waves and momentum,
	held to what the world's matter has in each pool. As in the game, the matter
	is the truth and this is the picture of it: nothing goes back.
*/
class FineWater
{
public:
	static constexpr int32_t NONE = INT32_MIN;
	// The stage as it stands. Its blocks, and each substance's colour.
	void sync(int32_t x, int32_t y, int32_t z, const std::vector<sandbox::Stage::Cell> &cells, const std::vector<sandbox::StageBlock> &blocks);
	// `dt`: real seconds; `pace`: how fast the matter's running against them.
	void step(float dt, float pace);
	// Something landed in it: thrown out round there (scene's frame from its corner, nodes).
	void splash(float x, float z, float speed);

	int width() const { return m_w; }
	int depth() const { return m_d; }
	int32_t shown(int c) const { return m_shown[c]; }  // its top, sixteenths over the stage's floor, or NONE
	int32_t floorAt(int c) const { return m_floor[c]; }
	uint32_t colourAt(int c) const { return m_colour[c]; }
	// What's gone over an edge since the last call: a column, and which way (x+, x−, z+, z−).
	struct Spill { int column, side; };
	std::vector<Spill> takeSpills();
	// Bubbles, if it's boiling: `dt` as step's, stopped when the scene's paused. Burst ones say where.
	struct Bubble { float x, y, z, speed, dies; bool burst; };
	void bubble(float dt, float pace);
	const std::vector<Bubble> &bubbles() const { return m_bubbles; }
	struct Burst { int column; float boil; };
	std::vector<Burst> takeBursts();
	float bubbleCount() const { return (float)m_bubbles.size(); }
	liquids::look::Surface lookOf(int c) const;
	double temperatureAt(int c) const { return m_temperature[c]; }
	float volume() const;   // nodes³ it holds
	float target() const;   // nodes³ the matter has
	int nodeOf(int c) const { return (c / m_w / 16) * m_nx + (c % m_w) / 16; }

private:
	void pools();
	void hold(float dt, float pace);
	void flow(float dt);

	int m_nx = 0, m_nz = 0, m_w = 0, m_d = 0;
	bool m_began = false, m_quiet = false;
	std::vector<int32_t> m_floor, m_shown, m_pool;
	std::vector<float> m_depth, m_targets; // by column; by node
	std::vector<uint32_t> m_colour;        // by column: its node's liquid, or what ran into it
	std::vector<std::array<float, 4>> m_out;
	std::vector<std::array<float, 4>> m_over; // by column and side: what's gone over a drop, towards the next spill
	std::vector<Spill> m_spills;
	std::vector<float> m_temperature;         // by column, carried as its colour is
	std::vector<float> m_boil;                // by node
	std::vector<Bubble> m_bubbles;
	std::vector<Burst> m_bursts;
	uint32_t m_seed = 1;
	float m_due = 0;
	float m_pools_in = 0, m_behind = 0, m_still = 0;
};

} // namespace shell
