// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "world.h"
#include "sandbox.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace sandbox {

using matter::State;

static const double BLOCK = 1.0;          // m: a world block
static const double LONGEST_STEP = 1.0;   // s
static const double WAKE = 2.0;           // K: how far heat must warm (or chill) an asleep block to wake it
static const double SETTLED = 1.0;        // K from the weather's temperature that counts as settled
static const double QUIET = 20;           // s settled before a block sleeps
static const double GONE = 0.1;           // share of solid or liquid below which a block has become air
static const double NOTICED = 1e-4;       // reactions per second that count as happening (for discovery)
// the most blocks awake at once (sprint 80: past it a block stays asleep, as
// if walled, rather than failing; the per-step budget is m_budget)
static const size_t MOST_AWAKE = 65536;
static const double SMOKE = 0.03;         // share of a block's gas, not the room's, that makes it smoke
static const double WIND = 25;            // how much faster gas leaves upward into open air than through still air
static const double REDRAW_LEVEL = 1.0 / 16; // of a block: a running level's change worth redrawing
static const double REDRAW_EVERY = 0.5;      // s: the most often a running level is redrawn
static const double DROP = 1e-4;          // of a block: less liquid than this in the air is a drop falling, not a puddle
static const double STIRRED = 1e-5;
static const double VACUUM = 1.0;         // Pa: the least pressure a body's air is run at       // m³ flowed in a step that keeps a block awake

WorldMatter::WorldMatter(const Opts &opts) :
	m_matter(opts.matter), m_ground(opts.ground)
{
	if (!m_ground.kind || !m_ground.become || !m_ground.ambient || !m_ground.level || !m_ground.room || !m_ground.face ||
			!m_ground.fill || !m_ground.setFill)
		throw SandboxError("the world's matter needs the ground's kind, become, ambient, level, room, face and fill");
	if (opts.airs.empty() || (opts.airs.size() > 1 && !m_ground.air))
		throw SandboxError("the world's matter needs an air, and with several, the ground's air to say which is where");
	for (const Air &a : opts.airs) {
		if (a.gravity <= 0)
			throw SandboxError("an air's body needs a pull");
		std::vector<double> air(m_matter.substances().size(), 0);
		double shares = 0;
		for (const auto &[id, share] : a.gases) {
			air[m_matter.index(id)] = share;
			shares += share;
		}
		if (!a.gases.empty() && std::fabs(shares - 1) > 1e-6)
			throw SandboxError("an air's gas shares must sum to 1");
		// no air: a trace of a pascal, so a block's gas has somewhere to go
		// (it leaves, and nothing comes back: a fire starves, water boils off)
		m_skies.push_back({ Physics({ opts.matter, std::max(a.pressure, VACUUM), BLOCK }), std::move(air), a.gravity, a.gases.empty() || a.pressure <= VACUUM });
	}
}

size_t WorldMatter::skyAt(Pos at) const
{
	if (m_skies.size() == 1)
		return 0;
	const size_t i = m_ground.air(at);
	if (i >= m_skies.size())
		throw SandboxError("the ground named an air the world's matter wasn't given");
	return i;
}

Room Room::open()
{
	Room r;
	std::fill(std::begin(r.layers), std::end(r.layers), uint16_t(256));
	return r;
}

Room Room::closed()
{
	Room r;
	std::fill(std::begin(r.layers), std::end(r.layers), uint16_t(0));
	return r;
}

uint32_t Room::total() const
{
	uint32_t n = 0;
	for (uint16_t l : layers)
		n += l;
	return n;
}

double Room::surface(double share) const
{
	const double all = total();
	if (all <= 0)
		return 0;
	double left = std::clamp(share, 0.0, 1.0) * all;
	for (int y = 0; y < 16; y++) {
		if (!layers[y])
			continue;
		if (left <= layers[y])
			return (y + left / layers[y]) / 16.0;
		left -= layers[y];
	}
	for (int y = 15; y >= 0; y--)
		if (layers[y])
			return (y + 1) / 16.0;
	return 0;
}

int Room::lowest() const
{
	for (int y = 0; y < 16; y++)
		if (layers[y])
			return y;
	return 16;
}

int64_t WorldMatter::key(Pos p)
{
	return ((int64_t)(p.x & 0x1FFFFF) << 42) | ((int64_t)(p.y & 0x1FFFFF) << 21) | (int64_t)(p.z & 0x1FFFFF);
}

int32_t WorldMatter::kind(const Spec &spec)
{
	const std::string &node = spec.node, &id = spec.matter;
	double share = spec.share;
	State state = spec.state;
	if (m_by_node.count(node))
		throw SandboxError(node + " already has its matter registered");
	if (share <= 0 || share > 1)
		throw SandboxError(node + ": a block's matter takes 0..1 of its volume");
	Kind k{ node, {}, id == "air", spec.form, spec.flowing, spec.vessel, spec.leveled, spec.loose, -1, state, 0, 0, 0 };
	if (k.vessel && !k.air)
		throw SandboxError(node + ": a vessel's matter is the air it holds");
	if ((k.leveled || k.loose) && (k.air || k.flowing))
		throw SandboxError(node + ": only a block of matter is levelled or loose (not air, nor a liquid the engine flows)");
	if (k.loose && state != State::Solid)
		throw SandboxError(node + ": a powder is solid");
	if (!k.air && state != State::Solid && state != State::Liquid)
		throw SandboxError(node + ": a block's matter is solid or liquid (gas fills the rest of it)");
	if (!k.air) {
		// the solid part: one substance or rock, or a mix of them; a rock is
		// its minerals, by volume from their shares by mass
		std::vector<std::pair<std::string, double>> parts = spec.parts;
		if (parts.empty())
			parts.push_back({ id, 1.0 });
		double shares = 0;
		for (const auto &[pid, part] : parts)
			shares += part;
		if (shares <= 0 || std::fabs(shares - 1) > 1e-3)
			throw SandboxError(node + ": a mix's shares must sum to 1");
		const auto &rocks = m_matter.rocks();
		for (const auto &[pid, part] : parts) {
			auto rock = std::find_if(rocks.begin(), rocks.end(), [&](const matter::RockKind &r) { return r.id == pid; });
			if (rock != rocks.end()) {
				double total = 0;
				for (const auto &p : rock->minerals)
					total += p.share / m_matter.substances()[p.substance].density_solid;
				for (const auto &p : rock->minerals)
					k.by_volume.push_back({ p.substance, share * part * p.share / m_matter.substances()[p.substance].density_solid / total });
			} else {
				k.by_volume.push_back({ m_matter.index(pid), share * part }); // throws on an unknown id
			}
		}
	}
	matter::Mixture sample = made(k, m_skies[0], 293.15);
	Physics::Main main = base().main(sample);
	k.main = main.substance;
	k.fill = main.fill;
	k.k = base().conductivity(sample);
	k.capacity = sample.heatCapacity();
	m_kinds.push_back(k);
	m_by_node[node] = (int32_t)m_kinds.size() - 1;
	return (int32_t)m_kinds.size() - 1;
}

const std::string &WorldMatter::node(int32_t kind) const
{
	if (kind < 0 || (size_t)kind >= m_kinds.size())
		throw SandboxError("no such kind of block");
	return m_kinds[kind].node;
}

double WorldMatter::density(int32_t kind) const
{
	if (kind < 0 || (size_t)kind >= m_kinds.size())
		throw SandboxError("no matter kind " + std::to_string(kind));
	const Kind &k = m_kinds[kind];
	double kg = 0;
	for (const auto &[s, volume] : k.by_volume)
		kg += volume * m_matter.substances()[s].density(k.state, 293.15, matter::ATMOSPHERE);
	return kg;
}

matter::Mixture WorldMatter::made(const Kind &k, const Sky &sky, double temperature, double level) const
{
	matter::Mixture b(m_matter);
	b.vessel = 1e-3;
	std::vector<std::pair<size_t, double>> parts = k.by_volume;
	for (auto &[s, share] : parts)
		share *= level; // a puddle a quarter deep is a quarter of the block's water
	sky.physics.fill(b, parts, temperature, sky.air, k.air ? std::nullopt : std::optional<State>(k.state));
	return b;
}

double WorldMatter::wakeAt(const Kind &k, const Sky &sky, double ambient) const
{
	if (k.air || k.main < 0)
		return ambient;
	// ice can't be at a summer's day temperature, nor water in a frost: they sit at their melting point
	const matter::Substance &s = m_matter.substances()[k.main];
	State there = s.state(ambient, std::max(sky.physics.pressure(), 1.0));
	if (there == k.state)
		return ambient;
	if ((k.state == State::Solid && there != State::Solid) || (k.state == State::Liquid && there == State::Solid))
		return s.meltingPoint(std::max(sky.physics.pressure(), s.triple.p));
	return s.boilingPoint(sky.physics.pressure()).value_or(ambient); // a liquid where it would boil
}

bool WorldMatter::same(const Kind &a, const Kind &b)
{
	return a.air == b.air && a.main == b.main && a.state == b.state && std::fabs(a.fill - b.fill) < 0.05;
}

WorldMatter::Cell *WorldMatter::wake(Pos at, double temperature)
{
	int32_t kind = m_ground.kind(at);
	auto it = m_cells.find(key(at));
	if (it != m_cells.end()) {
		if (kind == it->second.kind || (kind >= 0 && same(m_kinds[kind], m_kinds[it->second.kind])))
			return &it->second;
		m_cells.erase(it); // the block was replaced (a hot item placed where air was awake): start over as what's there
	}
	if (kind < 0 || m_cells.size() >= MOST_AWAKE)
		return nullptr; // nothing there, or too much awake already: it stays as it is
	const Kind &k = m_kinds[kind];
	const size_t air = skyAt(at);
	double base = wakeAt(k, m_skies[air], m_ground.ambient(at));
	// woken by heat or chill: as far from where it was as that took it
	double warmed = base + (temperature - m_ground.ambient(at));
	auto held = m_held.find(key(at));
	const bool kept = held != m_held.end() && (held->second.kind == kind || same(m_kinds[held->second.kind], k));
	// it kept its own matter while it slept, or it's what its kind is made of
	Cell c{ at, kind, kept ? std::move(held->second.mix) : asleep(at, k, warmed) };
	c.sky = (uint32_t)air;
	c.mix.temperature = warmed;
	c.loose = kept ? held->second.loose : k.loose;
	if (held != m_held.end())
		m_held.erase(held); // taken, or stale (the block was dug or built over while it slept)
	m_pending.erase(key(at));
	return &m_cells.emplace(key(at), std::move(c)).first->second;
}

// What an asleep block is: its kind's matter (a levelled one's share of it),
// and the liquid kept in its open space (sprint 81)
matter::Mixture WorldMatter::asleep(Pos at, const Kind &k, double temperature) const
{
	const Sky &here = sky(at);
	matter::Mixture b = made(k, here, temperature, k.leveled ? std::clamp(m_ground.level(at), 0.0, 1.0) : 1);
	const Liquid l = m_ground.fill(at);
	if (l.substance >= 0 && l.share > 0) {
		const matter::Substance &sub = m_matter.substances()[l.substance];
		const double m3 = l.share * here.physics.volume() * m_ground.room(at).total() / 4096.0;
		const double moles = m3 * sub.density(State::Liquid, temperature, here.physics.pressure()) / (sub.molar_mass / 1000.0);
		const double capacity = b.heatCapacity();
		b.add(l.substance, State::Liquid, moles);
		(void)capacity; // it's at the same temperature: nothing to mix
		// its gas gives up the room the liquid takes
		here.physics.balance(b, here.air, temperature);
	}
	return b;
}

void WorldMatter::heat(Pos at, double temperature)
{
	if (temperature <= 0)
		throw SandboxError("a temperature must be above 0 K");
	Cell *c = wake(at, m_ground.ambient(at));
	if (c)
		c->mix.temperature = std::max(c->mix.temperature, temperature);
}

void WorldMatter::touch(Pos at, double temperature)
{
	if (temperature <= 0)
		throw SandboxError("a temperature must be above 0 K");
	Cell *c = wake(at, m_ground.ambient(at));
	if (c) {
		c->mix.temperature = temperature;
		c->quiet = 0;
	}
}

void WorldMatter::warm(Pos at, double joules)
{
	Cell *c = wake(at, m_ground.ambient(at));
	const double capacity = c ? c->mix.heatCapacity() : 0;
	if (capacity > 0) {
		c->mix.temperature += joules / capacity;
		c->quiet = 0;
	}
}

void WorldMatter::blow(Pos at, double moles)
{
	Cell *c = wake(at, m_ground.ambient(at));
	if (!c || moles <= 0)
		return;
	double ambient = m_ground.ambient(at), per_kelvin = 0;
	for (size_t s = 0; s < sky(*c).air.size(); s++) {
		const matter::Substance &sub = m_matter.substances()[s];
		per_kelvin += sky(*c).air[s] * moles * sub.molar_mass / 1000.0 * sub.heat_capacity;
		c->mix.add(s, State::Gas, sky(*c).air[s] * moles);
	}
	double capacity = c->mix.heatCapacity() - per_kelvin;
	c->mix.temperature = (capacity * c->mix.temperature + per_kelvin * ambient) / (capacity + per_kelvin);
	c->quiet = 0;
}

void WorldMatter::add(Pos at, size_t substance, double moles, double temperature)
{
	if (substance >= m_matter.substances().size())
		throw SandboxError("no substance " + std::to_string(substance));
	if (moles <= 0 || temperature <= 0)
		throw SandboxError("matter added is some moles at a temperature above 0 K");
	Cell *c = wake(at, m_ground.ambient(at));
	if (!c)
		return;
	// in the state it's in as it arrives, then mixed: the two temperatures
	// met as their heat capacities say
	const matter::Substance &sub = m_matter.substances()[substance];
	const double per_kelvin = moles * sub.molar_mass / 1000.0 * sub.heat_capacity;
	const double capacity = c->mix.heatCapacity();
	c->mix.add(substance, sub.state(temperature, sky(*c).physics.pressure()), moles);
	c->mix.temperature = (capacity * c->mix.temperature + per_kelvin * temperature) / (capacity + per_kelvin);
	c->quiet = 0;
}

void WorldMatter::pour(Pos at, size_t substance, double moles, double temperature)
{
	add(at, substance, moles, temperature);
	if (auto it = m_cells.find(key(at)); it != m_cells.end())
		it->second.loose = true; // poured: it heaps and runs, not a block standing
}

void WorldMatter::disturb(Pos at)
{
	for (const Pos &d : { Pos{ 0, 0, 0 }, Pos{ 1, 0, 0 }, Pos{ -1, 0, 0 }, Pos{ 0, 1, 0 }, Pos{ 0, -1, 0 }, Pos{ 0, 0, 1 }, Pos{ 0, 0, -1 } }) {
		Pos p{ at.x + d.x, at.y + d.y, at.z + d.z };
		if (auto it = m_cells.find(key(p)); it != m_cells.end()) {
			it->second.quiet = 0;
			it->second.room_at = -1; // sculpted, dug beside: look at its open space again
			continue;
		}
		int32_t kind = m_ground.kind(p);
		if (kind < 0) {
			m_held.erase(key(p)); // dug out of the world's matter altogether
			continue;
		}
		const Kind &k = m_kinds[kind];
		// what can run or slide: a liquid of ours, a levelled block, a powder, or held matter
		if (k.leveled || k.loose || (!k.air && !k.flowing && k.state == State::Liquid) || m_held.count(key(p)))
			wake(p, m_ground.ambient(p));
	}
}

static const Pos SIDES[6] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };

// One awake block against its asleep neighbours: heat into solid ones (waking
// them once it matters), and the room's air through air ones.
void WorldMatter::exchange(Cell &c, double k, double dt)
{
	for (const Pos &d : SIDES) {
		Pos n{ c.pos.x + d.x, c.pos.y + d.y, c.pos.z + d.z };
		if (m_cells.count(key(n)))
			continue;
		int32_t kind = m_ground.kind(n);
		if (kind < 0)
			continue;
		const Kind &nk = m_kinds[kind];
		double ambient = m_ground.ambient(n);
		if (nk.air) {
			sky(c).physics.lose(c.mix, 1, ambient, dt);
			// open to the air (above or beside), a cloud of vapour (not a fire: that draws its own air) is carried
			// off faster than still air would (sprint 80: boiled spirit doesn't hang over its pool)
			const bool cloud = d.y >= 0 && std::fabs(c.activity) < 1 && unlike(c.mix, sky(c)) > SMOKE;
			sky(c).physics.breathe(c.mix, sky(c).air, ambient, d.y > 0, cloud ? dt * WIND : dt);
			continue;
		}
		if (d.y < 0 && nk.state == State::Liquid && !nk.flowing && !c.dense) {
			wake(n, ambient); // a liquid's surface under air that's awake gives off vapour into it (sprint 80)
			continue;
		}
		double keff = k + nk.k > 0 ? 2 * k * nk.k / (k + nk.k) : 0;
		double q = keff * sky(c).physics.area() / sky(c).physics.block() * (c.mix.temperature - ambient) * dt; // J into the neighbour
		c.mix.temperature = std::max(1.0, c.mix.temperature - q / c.capacity);
		if (!c.dense)
			q += sky(c).physics.radiateInto(c.mix, ambient, dt); // a flame shining on it
		double &pending = m_pending[key(n)];
		pending += q;
		if (std::fabs(pending) / nk.capacity > WAKE) {
			double warmed = ambient + pending / nk.capacity;
			m_pending.erase(key(n));
			wake(n, std::max(1.0, warmed));
		}
	}
}

// Change the block to the form of what now fills it, once its own kind no
// longer matches (a birch log stays a birch log until it's charcoal). Liquids
// and what's poured take levelled forms (sprint 80): a puddle, a heap, drawn
// by how full they are, at least a sixty-fourth so a drop can be seen.
void WorldMatter::reshape(Cell &c, bool exactly)
{
	if (m_kinds[c.kind].vessel)
		return; // a crucible of molten tin is still the crucible
	Physics::Main m = sky(c).physics.main(c.mix);
	const bool runs = m.substance >= 0 && m.fill > DROP && (m.state == State::Liquid || c.loose);
	auto plain = [&](const Kind &k) {
		return m.fill < GONE ? k.air : (!k.air && k.main == m.substance && k.state == m.state);
	};
	// a liquid or a heap: levelled where it has a levelled form (a powder only
	// its loose one), else its plain block, the old way (a pool of molten tin)
	auto levelled = [&](const Kind &k) {
		return !k.air && k.leveled && k.main == m.substance && k.state == m.state && (m.state == State::Liquid || k.loose);
	};
	auto draw = [&](int32_t kind) {
		double level = m_kinds[kind].leveled ? std::clamp(std::round(m.fill * 64) / 64, 1.0 / 64, 1.0) : 1;
		if (kind == c.kind && level == c.shown)
			return;
		// while it runs, its level is redrawn by sixteenths and at most twice a second:
		// each redraw remeshes the client's whole map block, and a spreading puddle
		// changes every step (sprint 80: the jolt as a block of water was placed)
		if (!exactly && kind == c.kind && c.shown >= 0 &&
				(std::fabs(level - c.shown) < REDRAW_LEVEL || m_clock - c.drawn_at < REDRAW_EVERY))
			return;
		c.drawn_at = m_clock;
		m_ground.become(c.pos, kind, level);
		c.kind = kind;
		c.shown = level;
	};
	auto first = [&](auto &&fits) -> int32_t {
		if (fits(m_kinds[c.kind]))
			return c.kind;
		for (size_t i = 0; i < m_kinds.size(); i++)
			if (m_kinds[i].form && fits(m_kinds[i]))
				return (int32_t)i;
		return -1;
	};
	// a liquid (sprint 81) isn't a block: it's kept in the open space of what's
	// there (air, a sculpted block) as its fill. Only full, where it has a plain
	// block (the sea's water), is it that block; a block of liquid it no longer
	// fills becomes the air its liquid lies in.
	if (runs && m.state == State::Liquid) {
		const int32_t full = m.fill > FULL ? fullForm(m.substance, m.state) : -1;
		const Kind &now = m_kinds[c.kind];
		if (full >= 0)
			draw(full);
		else if (!now.air && (now.state == State::Liquid || now.main == m.substance))
			draw(airForm()); // a block of liquid it no longer fills, or a block that melted (ice, tin): its liquid lies where it was
		showFill(c, exactly);
		return;
	}
	int32_t to = -1;
	if (runs)
		to = first(levelled);
	if (to < 0 && runs && m_kinds[c.kind].air && m.fill < GONE)
		return; // a little with no levelled form, in the air: it waits, awake, till there's enough to be its block
	if (to < 0)
		to = first(plain);
	if (to >= 0)
		draw(to);
	// no form for what fills it: the block stays as it is, awake, until one is registered
	showFill(c, exactly); // and what liquid lies in its open space (a pit in the sand)
}

int32_t WorldMatter::airForm() const
{
	for (size_t i = 0; i < m_kinds.size(); i++)
		if (m_kinds[i].air && m_kinds[i].form && !m_kinds[i].vessel)
			return (int32_t)i;
	throw SandboxError("no air registered as a form: a liquid has nothing to lie in");
}

void WorldMatter::showFill(Cell &c, bool exactly)
{
	const Kind &k = m_kinds[c.kind];
	int32_t main = -1;
	double share = 0, most = 0, m3 = 0;
	uint32_t colour = 0;
	const double open = sky(c).physics.volume() * m_ground.room(c.pos).total() / 4096.0;
	// a block of liquid is itself (the sea's): nothing is kept beside it
	if (!(!k.air && k.state == State::Liquid) && open > 0) {
		matter::Mixture liquid(m_matter);
		for (size_t s = 0; s < c.mix.amounts.size(); s++) {
			const double n = c.mix.amounts[s] > 0 ? c.mix.in(s, State::Liquid) : 0;
			if (n <= 0)
				continue;
			const matter::Substance &sub = m_matter.substances()[s];
			const double v = n * sub.molar_mass / 1000.0 / sub.density(State::Liquid, c.mix.temperature, sky(c).physics.pressure());
			m3 += v;
			liquid.add(s, State::Liquid, n);
			if (v > most) {
				most = v;
				main = (int32_t)s;
			}
		}
		share = std::min(1.0, m3 / open);
		if (share * 255 < 0.5)
			main = -1; // less than is kept: it's in the air as a drop
		else
			colour = liquid.colour();
	}
	if (main < 0)
		share = 0;
	if (share == c.filled && (main < 0 || colour == c.fill_colour))
		return;
	// while it runs, by thirty-seconds and four times a second (the client glides between)
	if (!exactly && c.filled >= 0 && main >= 0 && std::fabs(share - c.filled) < 1.0 / 32 && m_clock - c.filled_at < 0.25)
		return;
	if (c.filled < 0 && main < 0 && m_ground.fill(c.pos).substance < 0) {
		c.filled = 0; // nothing kept, nothing to keep
		return;
	}
	m_ground.setFill(c.pos, main, share, colour);
	c.filled = share;
	c.fill_colour = colour;
	c.filled_at = m_clock;
}

int32_t WorldMatter::fullForm(int32_t substance, State state) const
{
	for (size_t i = 0; i < m_kinds.size(); i++) {
		const Kind &k = m_kinds[i];
		if (k.form && !k.air && !k.leveled && !k.flowing && k.main == substance && k.state == state && state == State::Liquid)
			return (int32_t)i;
	}
	return -1;
}

double WorldMatter::unlike(const matter::Mixture &b, const Sky &sky) const
{
	double gas = sky.physics.gasMoles(b), most = 0;
	if (gas <= 0)
		return 0;
	for (size_t s = 0; s < sky.air.size(); s++)
		most = std::max(most, b.in(s, State::Gas) / gas - sky.air[s]);
	return most;
}

bool WorldMatter::settled(const Cell &c) const
{
	const Kind &k = m_kinds[c.kind];
	Physics::Main m = sky(c).physics.main(c.mix);
	if (k.vessel)
		return c.moved <= STIRRED; // it sleeps holding what it holds (sprint 80: kept)
	if (k.air) { // air settles once its gas is the room's again
		double gas = sky(c).physics.gasMoles(c.mix);
		for (size_t s = 0; s < sky(c).air.size(); s++)
			if (gas > 0 && std::fabs(c.mix.in(s, State::Gas) / gas - sky(c).air[s]) > 0.02)
				return false;
		// (sprint 81: open space holding a still liquid sleeps with it kept as its fill)
		return m.fill < GONE || (c.moved <= STIRRED && c.liquid > 0 && c.liquid >= c.solid);
	}
	if (c.moved > STIRRED)
		return false; // still running
	if (k.leveled)
		return m.substance == k.main && m.state == k.state;
	// close enough to sleep as the kind it is (melted ice takes 8% less room than the ice did)
	return m.substance == k.main && m.state == k.state && std::fabs(m.fill - k.fill) < 0.1;
}

void WorldMatter::step(double seconds)
{
	if (seconds <= 0)
		throw SandboxError("steps must move forward");
	const size_t reactions = m_matter.reactions().size();
	double left = seconds;
	while (left > 1e-9 && !m_cells.empty()) {
		double dt = std::min(left, LONGEST_STEP);
		// a block changed under us (dug, built over, flowed into) is no longer what we were running;
		// one swapped for another kind of the same matter (air for a flame's light) carries on
		for (auto it = m_cells.begin(); it != m_cells.end();) {
			int32_t now = m_ground.kind(it->second.pos);
			if (now == it->second.kind) {
				++it;
			} else if (now >= 0 && same(m_kinds[now], m_kinds[it->second.kind])) {
				it->second.kind = now;
				++it;
			} else {
				m_seen.erase(it->first);
				it = m_cells.erase(it);
			}
		}
		if (m_cells.empty())
			break;
		// the blocks stepped this time: all of them, or past the budget the
		// most active (reacting, flowing, far from the weather) first; the
		// rest wait, their chemistry's time owed to them
		std::vector<int64_t> ids;
		for (const auto &[id, c] : m_cells)
			ids.push_back(id);
		if (ids.size() > m_budget) {
			auto busy = [&](int64_t id) {
				const Cell &c = m_cells.at(id);
				return std::fabs(c.activity) + 1e6 * c.moved + c.capacity * std::fabs(c.mix.temperature - m_ground.ambient(c.pos)) * 1e-3 + c.owed * 1e3;
			};
			std::vector<std::pair<double, int64_t>> ranked;
			for (int64_t id : ids)
				ranked.push_back({ -busy(id), id });
			std::nth_element(ranked.begin(), ranked.begin() + m_budget, ranked.end());
			for (size_t i = m_budget; i < ranked.size(); i++)
				m_cells.at(ranked[i].second).owed += dt;
			ids.clear();
			for (size_t i = 0; i < m_budget; i++)
				ids.push_back(ranked[i].second);
		}
		std::sort(ids.begin(), ids.end()); // a fixed order: deterministic
		for (int64_t id : ids) {
			Cell &c = m_cells.at(id);
			c.k = sky(c).physics.conductivity(c.mix);
			c.capacity = c.mix.heatCapacity();
			c.dense = sky(c).physics.condensed(c.mix) > BLOCK * BLOCK * BLOCK / 2;
			dt = std::min(dt, std::max(1e-3, 0.25 * sky(c).physics.stable(c.mix, c.k)));
		}
		std::vector<double> rates;
		for (int64_t id : ids) {
			Cell &c = m_cells.at(id);
			// time it was passed over is made up (to the longest step: past that, it's lost)
			const double run = std::min(dt + c.owed, LONGEST_STEP);
			c.owed = 0;
			c.activity = sky(c).physics.react(c.mix, run, &rates);
			std::vector<bool> &seen = m_seen[id];
			seen.resize(reactions, false);
			double fastest = NOTICED;
			c.flame = 0;
			for (size_t j = 0; j < reactions; j++) {
				if (rates[j] > NOTICED && !seen[j]) {
					seen[j] = true;
					m_happened.push_back({ c.pos, j });
				}
				if (rates[j] > fastest && m_matter.reactions()[j].flame) {
					fastest = rates[j];
					c.flame = m_matter.reactions()[j].flame;
				}
			}
		}
		// awake neighbours with each other (each pair once), then with the asleep
		for (int64_t id : ids) {
			Cell &c = m_cells.at(id);
			for (const Pos &d : { Pos{ 1, 0, 0 }, Pos{ 0, 1, 0 }, Pos{ 0, 0, 1 } }) {
				auto it = m_cells.find(key({ c.pos.x + d.x, c.pos.y + d.y, c.pos.z + d.z }));
				if (it == m_cells.end())
					continue;
				sky(c).physics.conduct(c.mix, it->second.mix, c.k, it->second.k, c.capacity, it->second.capacity, dt);
				if (c.dense && it->second.dense)
					continue; // no light or gas between two solids
				sky(c).physics.radiate(c.mix, it->second.mix, dt);
				sky(c).physics.mix(c.mix, it->second.mix, d.y > 0, dt);
			}
		}
		for (int64_t id : ids) {
			auto it = m_cells.find(id);
			if (it != m_cells.end())
				exchange(it->second, it->second.k, dt);
		}
		// gas a block can't hold leaves upward (or sideways) into air; space left draws it in
		// (a still solid, its chemistry not run: nothing in it changed but its
		// temperature, so it has no gas to shed, nothing to become or pour)
		auto quiet = [](const Cell &c) { return c.dense && c.mix.still(); };
		for (int64_t id : ids) {
			Cell &c = m_cells.at(id);
			if (quiet(c))
				continue;
			for (const Pos &d : { Pos{ 0, 1, 0 }, Pos{ 1, 0, 0 }, Pos{ -1, 0, 0 }, Pos{ 0, 0, 1 }, Pos{ 0, 0, -1 }, Pos{ 0, -1, 0 } }) {
				Pos n{ c.pos.x + d.x, c.pos.y + d.y, c.pos.z + d.z };
				auto it = m_cells.find(key(n));
				if (it != m_cells.end() && sky(c).physics.room(it->second.mix) > 0) {
					sky(c).physics.balance(c.mix, it->second.mix);
					break;
				}
				int32_t kind = it == m_cells.end() ? m_ground.kind(n) : -1;
				if (kind >= 0 && m_kinds[kind].air) {
					// gas pushed out that isn't the room's air (smoke, a fire's fuel gas) wakes the
					// air it flows into, so it can burn there as a flame instead of vanishing
					// (into a vacuum it spreads to nothing: no flame)
					if (!sky(c).vacuum && sky(c).physics.gasMoles(c.mix) > sky(c).physics.room(c.mix) * 1.0001 && unlike(c.mix, sky(c)) > SMOKE) {
						Cell *air = wake(n, m_ground.ambient(n));
						if (air) {
							sky(c).physics.balance(c.mix, air->mix);
							break;
						}
					}
					sky(c).physics.balance(c.mix, sky(c).air, m_ground.ambient(n));
					break;
				}
			}
		}
		// blocks become what fills them (ice that melted is water now)
		for (int64_t id : ids) {
			auto it = m_cells.find(id);
			if (it != m_cells.end() && !quiet(it->second))
				reshape(it->second);
		}
		// liquids fall and run, powders heap and slide, liquids give off vapour
		// to the air over them (sprint 80: pouring.cpp); then they show their level
		flow(ids, dt);
		for (int64_t id : ids) {
			auto it = m_cells.find(id);
			if (it == m_cells.end())
				continue;
			if (it->second.liquid > 0)
				evaporate(it->second, dt);
			if (it->second.moved > 0)
				reshape(it->second);
		}
		// settled blocks sleep
		for (int64_t id : ids) {
			auto it = m_cells.find(id);
			if (it == m_cells.end())
				continue;
			Cell &c = it->second;
			bool calm = std::fabs(c.mix.temperature - m_ground.ambient(c.pos)) < SETTLED && std::fabs(c.activity) < 0.01;
			c.quiet = calm ? c.quiet + dt : 0;
			if (c.quiet > QUIET && settled(c))
				sleep(it);
		}
		left -= dt;
		m_clock += dt;
	}
}

// A block goes back to sleep: as its kind, or, where its matter isn't just
// what its kind is made of (a puddle of brine, more than its level shows, a
// crucible's bronze), holding its own.
void WorldMatter::sleep(std::unordered_map<int64_t, Cell>::iterator it)
{
	Cell &c = it->second;
	reshape(c, true); // asleep, it's drawn as full as it is (its level, its fill)
	const Kind &k = m_kinds[c.kind];
	bool plain = !k.vessel && !c.loose;
	if (plain) {
		const matter::Mixture as = asleep(c.pos, k, c.mix.temperature);
		double total = 0, off = 0;
		for (size_t s = 0; s < c.mix.amounts.size(); s++)
			for (State st : { State::Solid, State::Liquid }) {
				total += as.in(s, st);
				off += std::fabs(c.mix.in(s, st) - as.in(s, st));
			}
		plain = off <= 1e-3 * total + 1e-6;
	}
	if (!plain) {
		m_held.erase(it->first);
		m_held.emplace(it->first, Held{ c.pos, c.kind, std::move(c.mix), c.loose });
	}
	m_seen.erase(it->first);
	m_cells.erase(it);
}

double WorldMatter::capacity(Pos at) const
{
	auto it = m_cells.find(key(at));
	if (it != m_cells.end())
		return it->second.mix.heatCapacity();
	int32_t kind = m_ground.kind(at);
	return kind < 0 ? -1 : m_kinds[kind].capacity;
}

double WorldMatter::temperature(Pos at) const
{
	auto it = m_cells.find(key(at));
	return it != m_cells.end() ? it->second.mix.temperature : m_ground.ambient(at);
}

std::vector<WorldMatter::Awake> WorldMatter::awake(Pos lo, Pos hi) const
{
	std::vector<Awake> out;
	for (const auto &[id, c] : m_cells) {
		const Pos &p = c.pos;
		if (p.x < lo.x || p.y < lo.y || p.z < lo.z || p.x > hi.x || p.y > hi.y || p.z > hi.z)
			continue;
		Physics::Main m = sky(c).physics.main(c.mix);
		Awake a{ p, c.mix.temperature, c.activity, m.substance, m.state, m.fill, -1, 0, c.flame };
		double gas = sky(c).physics.gasMoles(c.mix), unlike = 0;
		if (gas > 0)
			for (size_t s = 0; s < sky(c).air.size(); s++) {
				double excess = c.mix.in(s, State::Gas) / gas - sky(c).air[s];
				if (c.mix.amounts[s] > 0 && excess > unlike) {
					unlike = excess;
					a.vapour = (int32_t)s;
					a.haze = std::min(1.0, excess);
				}
			}
		out.push_back(a);
	}
	return out;
}

std::optional<WorldMatter::Awake> WorldMatter::inspect(Pos at) const
{
	if (m_cells.count(key(at))) {
		std::vector<Awake> one = awake(at, at);
		return one.front();
	}
	int32_t kind = m_ground.kind(at);
	if (kind < 0)
		return std::nullopt;
	const Kind &k = m_kinds[kind];
	return Awake{ at, wakeAt(k, sky(at), m_ground.ambient(at)), 0, k.air ? -1 : k.main, k.state, k.air ? 0 : k.fill, -1, 0 };
}

std::optional<matter::Mixture> WorldMatter::mixture(Pos at) const
{
	if (auto it = m_cells.find(key(at)); it != m_cells.end())
		return it->second.mix;
	int32_t kind = m_ground.kind(at);
	if (kind < 0)
		return std::nullopt;
	if (auto held = m_held.find(key(at)); held != m_held.end() && held->second.kind == kind)
		return held->second.mix;
	const Kind &k = m_kinds[kind];
	return made(k, sky(at), wakeAt(k, sky(at), m_ground.ambient(at)), k.leveled ? m_ground.level(at) : 1);
}

std::vector<std::pair<Pos, size_t>> WorldMatter::happened()
{
	std::vector<std::pair<Pos, size_t>> out;
	out.swap(m_happened);
	return out;
}

// ---- saving: the awake blocks, by node name so the registry's order
// doesn't matter, and their amounts by substance id so the matter table can
// grow (version 2, sprint 74: version 1 had only how many there were, and
// a substance added refused every world). Version 4 (sprint 80): whether
// each is loose, its amounts by state, its flow and owed time, and the
// asleep blocks that hold their own matter (3 was this sprint's, in progress).

static const char MAGIC[8] = { 'T', 'F', 'W', 'M', 0, 0, 0, 4 };
static const char MAGIC_3[8] = { 'T', 'F', 'W', 'M', 0, 0, 0, 3 };
static const char MAGIC_2[8] = { 'T', 'F', 'W', 'M', 0, 0, 0, 2 };
static const char MAGIC_1[8] = { 'T', 'F', 'W', 'M', 0, 0, 0, 1 };

template <typename T>
static void put(std::string &out, const T &v)
{
	out.append(reinterpret_cast<const char *>(&v), sizeof v);
}

template <typename T>
static T take(const std::string &in, size_t &at)
{
	if (at + sizeof(T) > in.size())
		throw SandboxError("world matter: cut short");
	T v;
	std::memcpy(&v, in.data() + at, sizeof v);
	at += sizeof v;
	return v;
}

std::string WorldMatter::save() const
{
	std::string out(MAGIC, sizeof MAGIC);
	put(out, (uint32_t)m_matter.substances().size());
	for (const auto &sub : m_matter.substances()) {
		put(out, (uint32_t)sub.id.size());
		out += sub.id;
	}
	auto block = [&](Pos p, int32_t kind, const matter::Mixture &mix, bool loose) {
		put(out, p);
		const std::string &name = m_kinds[kind].node;
		put(out, (uint32_t)name.size());
		out += name;
		put(out, (uint8_t)loose);
		put(out, mix.temperature);
		// each in each state (version 3: what's half melted stays half melted, and runs as it did)
		for (size_t s = 0; s < mix.amounts.size(); s++)
			for (State st : { State::Solid, State::Liquid, State::Gas })
				put(out, mix.amounts[s] > 0 ? std::max(0.0, mix.in(s, st)) : 0.0); // (rounding leaves a −1e-125 now and then)
	};
	put(out, (uint32_t)m_cells.size());
	std::vector<int64_t> ids;
	for (const auto &[id, c] : m_cells)
		ids.push_back(id);
	std::sort(ids.begin(), ids.end());
	for (int64_t id : ids) {
		const Cell &c = m_cells.at(id);
		block(c.pos, c.kind, c.mix, c.loose);
		for (double f : c.flux) // where it was running: a pool loaded sloshes on as it was
			put(out, f);
		put(out, c.owed);
	}
	put(out, (uint32_t)m_held.size());
	ids.clear();
	for (const auto &[id, h] : m_held)
		ids.push_back(id);
	std::sort(ids.begin(), ids.end());
	for (int64_t id : ids) {
		const Held &h = m_held.at(id);
		block(h.pos, h.kind, h.mix, h.loose);
	}
	return out;
}

void WorldMatter::load(const std::string &in)
{
	const bool first = in.size() >= sizeof MAGIC_1 && std::memcmp(in.data(), MAGIC_1, sizeof MAGIC_1) == 0;
	const bool second = in.size() >= sizeof MAGIC_2 && std::memcmp(in.data(), MAGIC_2, sizeof MAGIC_2) == 0;
	if (in.size() >= sizeof MAGIC_3 && std::memcmp(in.data(), MAGIC_3, sizeof MAGIC_3) == 0)
		throw SandboxError("world matter: saved by a sprint 80 build in progress (version 3), whose layout "
				"never shipped; delete the world's matter.bin to start its matter afresh");
	if (!first && !second && (in.size() < sizeof MAGIC || std::memcmp(in.data(), MAGIC, sizeof MAGIC) != 0))
		throw SandboxError("world matter: not a world matter file (or an older version)");
	size_t at = sizeof MAGIC;
	// where each saved substance is in the table now
	const uint32_t saved = take<uint32_t>(in, at);
	std::vector<size_t> now(saved);
	if (first) {
		// version 1: the table as it was then; substances have only been
		// added at its end since, so the first are the same
		if (saved > m_matter.substances().size())
			throw SandboxError("world matter: saved with a matter table that had more in it");
		for (uint32_t s = 0; s < saved; s++)
			now[s] = s;
	} else {
		for (uint32_t s = 0; s < saved; s++) {
			const uint32_t length = take<uint32_t>(in, at);
			if (at + length > in.size())
				throw SandboxError("world matter: cut short");
			const std::string id = in.substr(at, length);
			at += length;
			const auto &all = m_matter.substances();
			const auto it = std::find_if(all.begin(), all.end(), [&](const auto &x) { return x.id == id; });
			if (it == all.end())
				throw SandboxError("world matter: saved with " + id + ", which isn't a substance now");
			now[s] = it - all.begin();
		}
	}
	// one block: its kind, whether it's loose, and what's in it
	auto block = [&](int32_t &kind, bool &loose) {
		uint32_t length = take<uint32_t>(in, at);
		if (at + length > in.size())
			throw SandboxError("world matter: cut short");
		std::string name = in.substr(at, length);
		at += length;
		auto found = m_by_node.find(name);
		if (found == m_by_node.end())
			throw SandboxError("world matter: a block of " + name + ", which has no matter registered now");
		kind = found->second;
		loose = !first && !second && take<uint8_t>(in, at) != 0;
		matter::Mixture mix = made(m_kinds[kind], m_skies[0], 293.15);
		double t = take<double>(in, at);
		// what it's made of, as saved: none of anything else
		for (size_t s = 0; s < mix.amounts.size(); s++)
			for (State st : { State::Solid, State::Liquid, State::Gas })
				mix.take(s, st, mix.amounts[s]);
		mix.temperature = t;
		for (uint32_t k = 0; k < saved; k++) {
			const size_t s = now[k];
			if (first || second) {
				const double n = take<double>(in, at);
				if (n > 0)
					mix.add(s, m_matter.substances()[s].state(t, std::max(base().pressure(), 1.0)), n);
				continue;
			}
			for (State st : { State::Solid, State::Liquid, State::Gas }) {
				const double n = take<double>(in, at);
				if (n > 0)
					mix.add(s, st, n);
			}
		}
		return mix;
	};
	uint32_t count = take<uint32_t>(in, at);
	std::unordered_map<int64_t, Cell> cells;
	for (uint32_t k = 0; k < count; k++) {
		Pos p = take<Pos>(in, at);
		int32_t kind;
		bool loose;
		matter::Mixture mix = block(kind, loose);
		Cell c{ p, kind, std::move(mix) };
		c.sky = (uint32_t)skyAt(p);
		c.loose = loose;
		if (!first && !second) {
			for (double &f : c.flux)
				f = take<double>(in, at);
			c.owed = take<double>(in, at);
		}
		cells.emplace(key(p), std::move(c));
	}
	std::unordered_map<int64_t, Held> held;
	if (!first && !second) {
		const uint32_t asleep = take<uint32_t>(in, at);
		for (uint32_t k = 0; k < asleep; k++) {
			Pos p = take<Pos>(in, at);
			int32_t kind;
			bool loose;
			matter::Mixture mix = block(kind, loose);
			held.emplace(key(p), Held{ p, kind, std::move(mix), loose });
		}
	}
	if (at != in.size())
		throw SandboxError("world matter: trailing bytes");
	m_cells = std::move(cells);
	m_held = std::move(held);
}

} // namespace sandbox
