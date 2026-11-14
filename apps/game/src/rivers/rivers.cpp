// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "rivers.h"
#include "terragen/grid.h"
#include "terragen/sphere.h"
#include "terragen/world.h"
#include "weather/weather.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace rivers {

static double toDouble(terragen::fix v) { return (double)v / terragen::ONE; }

static const char MAGIC[8] = { 'T', 'F', 'R', 'I', 'V', 'E', 'R', '3' };

Network::Network(std::vector<int32_t> receiver, std::vector<double> area, std::vector<double> travel, double memory) :
	m_receiver(std::move(receiver)), m_area(std::move(area)), m_travel(std::move(travel)), m_memory(memory)
{
	const size_t n = m_receiver.size();
	if (m_area.size() != n || m_travel.size() != n)
		throw RiversError("rivers: a network's cells disagree in number");
	// upstream first: a cell once all that drains into it is placed
	std::vector<int32_t> in(n, 0);
	for (int32_t r : m_receiver)
		if (r >= 0)
			in.at(r)++;
	for (size_t i = 0; i < n; i++)
		if (in[i] == 0)
			m_order.push_back((int32_t)i);
	for (size_t k = 0; k < m_order.size(); k++) {
		const int32_t r = m_receiver[m_order[k]];
		if (r >= 0 && --in[r] == 0)
			m_order.push_back(r);
	}
	if (m_order.size() != n)
		throw RiversError("rivers: the drainage runs in a loop");
	m_to_sea_time.assign(n, 0);
	for (auto k = m_order.rbegin(); k != m_order.rend(); ++k) // downstream first
		m_to_sea_time[*k] = m_travel[*k] + (m_receiver[*k] >= 0 ? m_to_sea_time[m_receiver[*k]] : 0);
	m_store.assign(n, 0);
	m_ground.assign(n, 0);
	m_out.assign(n, 0);
	m_mean.assign(n, 0);
	m_own.assign(n, 0);
	m_own_mean.assign(n, 0);
}

Network Network::ofGrid(const terragen::Grid &grid, const terragen::Sphere &sphere, double radius, double speed,
		double memory)
{
	const int32_t n = grid.size();
	const double mean_area = 4 * M_PI * radius * radius / n;
	const double across = toDouble(sphere.spacing()) * radius;
	std::vector<double> area(n), travel(n);
	for (int32_t i = 0; i < n; i++) {
		area[i] = toDouble(sphere.area(i)) * mean_area;
		travel[i] = across / speed;
	}
	return Network(grid.receiver, std::move(area), std::move(travel), memory);
}

void Network::settle(const std::vector<Shed> &shed)
{
	std::vector<double> in(size(), 0);
	for (int32_t i : m_order) {
		const Shed &w = shed.at(i);
		const double own = std::max(0.0, w.quick) + std::max(0.0, w.soak);
		m_ground[i] = std::max(0.0, w.soak) * m_area[i] * GROUND_TIME;
		const double q = std::max(0.0, in[i] + (own - std::max(0.0, w.lose)) * m_area[i]);
		m_own[i] = m_own_mean[i] = own;
		m_out[i] = m_mean[i] = q;
		m_store[i] = q * m_travel[i];
		if (m_receiver[i] >= 0)
			in[m_receiver[i]] += q;
	}
}

// A linear reservoir (letting go store / k) fed `in` (m³/s) through `dt`,
// solved exactly, so a step longer than k is as stable as a short one:
// its store after, and what it let go meanwhile (m³)
static double reservoir(double &store, double in, double k, double dt)
{
	const double before = store;
	store = in * k + (before - in * k) * std::exp(-dt / k);
	return before + in * dt - store;
}

void Network::step(double dt, const std::vector<Shed> &shed)
{
	std::vector<double> in(size(), 0); // m³/s from upstream, through the step
	const double remember = 1 - std::exp(-dt / m_memory);
	for (int32_t i : m_order) {
		const Shed &w = shed.at(i);
		const double quick = std::max(0.0, w.quick) * m_area[i], soak = std::max(0.0, w.soak) * m_area[i];
		m_rained += (quick + soak) * dt;
		const double from_ground = reservoir(m_ground[i], soak, GROUND_TIME, dt) / dt;
		m_own[i] = (quick + from_ground) / m_area[i];
		m_own_mean[i] += (m_own[i] - m_own_mean[i]) * remember;
		const double lose = std::max(0.0, w.lose) * m_area[i];
		double gone;
		if (lose > 0) {
			// a lake: what comes in less what evaporates. Short of it, its store
			// falls to nothing (at `dry`), and then the air takes all that comes
			// in and nothing goes on
			const double feed = in[i] + quick + from_ground, net = feed - lose, k = m_travel[i], before = m_store[i];
			double dry = dt;
			if (net < 0)
				dry = std::min(dt, k * std::log((before - net * k) / (-net * k)));
			if (dry >= dt) {
				gone = reservoir(m_store[i], net, k, dt);
				m_evaporated += lose * dt;
			} else {
				m_store[i] = 0;
				gone = before + net * dry;
				m_evaporated += lose * dry + feed * (dt - dry);
			}
		} else {
			gone = reservoir(m_store[i], in[i] + quick + from_ground, m_travel[i], dt);
		}
		m_out[i] = m_store[i] / m_travel[i];
		m_mean[i] += (m_out[i] - m_mean[i]) * remember;
		if (m_receiver[i] >= 0)
			in[m_receiver[i]] += gone / dt;
		else
			m_to_sea += gone;
	}
}

double Network::stored() const
{
	double s = 0;
	for (size_t i = 0; i < m_store.size(); i++)
		s += m_store[i] + m_ground[i];
	return s;
}

std::string Network::save() const
{
	std::string out(MAGIC, sizeof MAGIC);
	auto put = [&](const void *p, size_t bytes) { out.append((const char *)p, bytes); };
	const uint32_t n = (uint32_t)size();
	put(&n, sizeof n);
	put(&m_to_sea, sizeof m_to_sea);
	put(&m_rained, sizeof m_rained);
	put(&m_evaporated, sizeof m_evaporated);
	for (const std::vector<double> *v : { &m_store, &m_ground, &m_out, &m_mean, &m_own, &m_own_mean })
		put(v->data(), v->size() * sizeof(double));
	return out;
}

void Network::load(const std::string &bytes)
{
	const size_t n = size(), want = sizeof MAGIC + sizeof(uint32_t) + 3 * sizeof(double) + 6 * n * sizeof(double);
	if (bytes.size() != want || bytes.compare(0, sizeof MAGIC, MAGIC, sizeof MAGIC) != 0)
		throw RiversError("rivers: not this network's saved state");
	const char *p = bytes.data() + sizeof MAGIC;
	uint32_t count;
	std::memcpy(&count, p, sizeof count);
	p += sizeof count;
	if (count != n)
		throw RiversError("rivers: saved for a network of another size");
	std::memcpy(&m_to_sea, p, sizeof m_to_sea);
	p += sizeof m_to_sea;
	std::memcpy(&m_rained, p, sizeof m_rained);
	p += sizeof m_rained;
	std::memcpy(&m_evaporated, p, sizeof m_evaporated);
	p += sizeof m_evaporated;
	for (std::vector<double> *v : { &m_store, &m_ground, &m_out, &m_mean, &m_own, &m_own_mean }) {
		std::memcpy(v->data(), p, n * sizeof(double));
		p += n * sizeof(double);
	}
}

Shed shed(double rain, double soil)
{
	// a little runs off even dry ground (rock, a downpour faster than it soaks
	// in); the rest by how full the ground's bucket is, the last of it most.
	// Of what soaks in, RECHARGE gets past the roots to the ground water.
	static const double RECHARGE = 0.3;
	const double s = std::clamp(soil, 0.0, 1.0);
	const double quick = 0.05 + 0.85 * s * s;
	return Shed(rain * quick, rain * (1 - quick) * RECHARGE);
}

// A lake cell's: the rain on it all its water's (open water soaks nothing
// up), and what the weather's air over it takes off it
static Shed lakeShed(const weather::Weather &weather, const terragen::Sphere &sphere, int32_t cell, double rain, double now)
{
	const fluid::Sample air = weather.sample(terragen::Place(sphere.centre(cell)), 0, now);
	return Shed(rain, 0, weather::openWaterEvaporation(air) / 1000 / 3600);
}

std::vector<Shed> climate(const terragen::Grid &grid, const weather::Weather *weather, const terragen::Sphere *sphere)
{
	std::vector<Shed> out(grid.size());
	for (int32_t i = 0; i < grid.size(); i++) {
		if (grid.base[i] < 0)
			continue;
		const double rain = weather::Weather::cellRainMean(grid, i) / 1000 / 3600;
		out[i] = weather && sphere && grid.lake[i] ? lakeShed(*weather, *sphere, i, rain, weather->now()) : shed(rain, 0.5);
	}
	return out;
}

std::vector<Shed> runoff(const weather::Weather &weather, const terragen::Grid &grid, const terragen::Sphere &sphere,
		double now)
{
	std::vector<Shed> out(grid.size());
	for (int32_t i = 0; i < grid.size(); i++) {
		if (grid.base[i] < 0)
			continue; // the sea's
		double soil = 0;
		const double mm_h = weather.cellRain(grid, sphere.centre(i), i, now, soil);
		out[i] = grid.lake[i] ? lakeShed(weather, sphere, i, mm_h / 1000 / 3600, now) : shed(mm_h / 1000 / 3600, soil);
	}
	return out;
}

void force(std::vector<Shed> &shed, const terragen::Sphere &sphere, const Forcing &f)
{
	const terragen::Dir c = f.centre.dir();
	const double cx = toDouble(c.x), cy = toDouble(c.y), cz = toDouble(c.z);
	const double reach = std::cos(f.radius);
	for (int32_t i = 0; i < (int32_t)shed.size(); i++) {
		const terragen::Dir &d = sphere.centre(i);
		if (cx * toDouble(d.x) + cy * toDouble(d.y) + cz * toDouble(d.z) >= reach)
			shed[i] = f.rain > 0 ? rivers::shed(f.rain, 1) : Shed();
	}
}

Flow flowAt(const Network &net, const terragen::Sphere &sphere, const terragen::Ground &ground,
		const terragen::Place &at)
{
	Flow f;
	if (ground.kind == terragen::Water::Lake)
		return f; // stands at its level (its outlet's flow, basins')
	const bool grid_river = ground.river_cell >= 0 && ground.river_width > 0 && ground.river_dist * 2 < ground.river_width &&
			ground.water == ground.river_level;
	double depth; // blocks: as generated, at its mean flow
	if (grid_river) {
		const int32_t c = ground.river_cell;
		f.discharge = net.discharge(c);
		f.ratio = net.mean(c) > 0 ? net.discharge(c) / net.mean(c) : 1;
		f.to_sea = net.toSeaTime(c);
		depth = std::max(1.0, (double)(ground.water - ground.terrain));
	} else {
		const int32_t c = sphere.cell(at.dir());
		f.ratio = net.shedding_mean(c) > 0 ? net.shedding(c) / net.shedding_mean(c) : 1;
		// what its own catchment sheds: its share of its cell's (terragen's
		// channels: what it drains, in cells); not known, all of the cell's
		f.stream = true;
		f.share = ground.stream_cells > 0 ? toDouble(ground.stream_cells) : 1;
		f.discharge = net.shedding(c) * net.area(c) * f.share;
		f.to_sea = net.toSeaTime(c);
		depth = 1;
	}
	f.ratio = std::clamp(f.ratio, 0.0, 20.0);
	// the generated level is its mean flow's: deeper or shallower by the ratio,
	// never under its bed, never over the block above's top (a flood)
	f.surge = std::max(-depth, depth * (std::pow(f.ratio, 0.4) - 1));
	f.rise = std::min(f.surge, 15.0 / 16);
	const double slope = std::max(0.0, (double)ground.river_slope / terragen::ONE);
	f.speed = std::clamp(0.6 + 20 * std::sqrt(slope), 0.6, 3.0) * std::pow(f.ratio, 0.3);
	// what it carries in the blocks: its channel's width, its water's depth
	// now and its speed. The network's discharge is its planet's, in real
	// metres; the blocks squeeze that planet thousands of times across, so
	// what a dam holds back is what the channel the player sees carries
	const double width = grid_river ? std::max(1.0, toDouble(ground.river_width)) : 1;
	f.local = width * std::max(0.0, depth + f.surge) * f.speed;
	return f;
}

} // namespace rivers
