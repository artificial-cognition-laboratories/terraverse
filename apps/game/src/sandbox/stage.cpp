// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "stage.h"
#include "sandbox.h"
#include <algorithm>
#include <cmath>

namespace sandbox {

static constexpr double NEAR_BOILING = 25; // K under it: the first bubbles

// The world's own steps are short: a long one (the scene at ×600) is many of them.
static constexpr double LONGEST_STEP = 0.25;

Stage::Stage(const Opts &opts) :
	m_matter(opts.matter), m_blocks(opts.blocks), m_x(opts.x), m_y(opts.y), m_z(opts.z), m_ambient(opts.ambient), m_pressure(opts.pressure)
{
	if (m_x < 1 || m_y < 1 || m_z < 1 || m_x > 16 || m_y > 16 || m_z > 16)
		throw SandboxError("a stage is 1 to 16 blocks a side");
	const size_t cells = (size_t)m_x * m_y * m_z;
	m_levels.assign(cells, 1);
	m_fills.assign(cells, Liquid{});
	m_colours.assign(cells, 0);

	WorldMatter::Ground ground{
		[this](Pos p) -> int32_t {
			if (inside(p))
				return m_kinds[index(p)];
			// the top's open to the room; the sides and floor hold what's in it
			return p.y >= m_y && p.x >= 0 && p.z >= 0 && p.x < m_x && p.z < m_z ? m_air : -1;
		},
		[this](Pos p, int32_t kind, double level) {
			if (!inside(p))
				return; // gone up and out into the room
			m_kinds[index(p)] = kind;
			m_levels[index(p)] = level;
		},
		[this](Pos) { return m_ambient; },
		[this](Pos p) { return inside(p) ? m_levels[index(p)] : 1.0; },
		// as the world's (openOf): air, a liquid and a levelled heap have room in them; a solid block has none
		[this](Pos p) { return roomy(p) ? Room::open() : Room::closed(); },
		[this](Pos p, int) { return roomy(p) ? (uint16_t)0xFFFF : (uint16_t)0; },
		[this](Pos p) { return inside(p) ? m_fills[index(p)] : Liquid{}; },
		[this](Pos p, int32_t substance, double share, uint32_t colour) {
			if (!inside(p))
				return;
			m_fills[index(p)] = Liquid{ substance, share };
			m_colours[index(p)] = colour;
		},
		nullptr,
	};
	m_world = std::make_unique<WorldMatter>(WorldMatter::Opts{ m_matter,
		{ WorldMatter::Air{ opts.pressure, opts.gravity, opts.air } }, std::move(ground) });
	for (size_t i = 0; i < m_blocks.size(); i++) {
		const int32_t kind = m_world->kind(m_blocks[i].spec);
		if (m_kindOf.size() <= (size_t)kind)
			m_kindOf.resize(kind + 1, -1);
		m_kindOf[kind] = (int32_t)i;
		m_byName[m_blocks[i].spec.node] = kind;
		if (m_blocks[i].spec.matter == "air" && !m_blocks[i].spec.vessel && m_air < 0)
			m_air = kind;
	}
	if (m_air < 0)
		throw SandboxError("a stage needs the game's air among its blocks");
	m_kinds.assign(cells, m_air);
}

std::vector<StageBlock> Stage::read(const Json::Value &scene)
{
	std::vector<StageBlock> out;
	for (const Json::Value &b : scene) {
		StageBlock block;
		block.spec.node = b["node"].asString();
		block.spec.matter = b["matter"].asString();
		block.spec.share = b["share"].asDouble();
		block.spec.state = b["liquid"].asBool() ? matter::State::Liquid : matter::State::Solid;
		block.spec.form = b["form"].asBool();
		block.spec.flowing = b["flowing"].asBool();
		block.spec.vessel = b["vessel"].asBool();
		block.spec.leveled = b["leveled"].asBool();
		block.spec.loose = b["loose"].asBool();
		for (Json::ArrayIndex t = 0; t < 3; t++)
			block.tiles[t] = b["tiles"][t].asString();
		block.friction = b.get("friction", 0.6).asFloat();
		for (Json::ArrayIndex k = 0; k < b["pile"].size(); k++)
			block.pile[k] = b["pile"][k].asInt();
		for (const Json::Value &part : b["boxes"]) {
			std::array<float, 6> box;
			for (Json::ArrayIndex k = 0; k < 6; k++)
				box[k] = part[k].asFloat();
			block.boxes.push_back(box);
		}
		out.push_back(std::move(block));
	}
	return out;
}

double Stage::moles(const std::string &substance) const
{
	const size_t k = m_matter.index(substance);
	double total = 0;
	for (int y = 0; y < m_y; y++)
		for (int z = 0; z < m_z; z++)
			for (int x = 0; x < m_x; x++)
				if (const std::optional<matter::Mixture> mix = m_world->mixture({ x, y, z }))
					total += mix->amounts[k];
	return total;
}

bool Stage::roomy(Pos p) const
{
	if (!inside(p))
		return p.y >= m_y; // the room above; the sides and floor hold what's in it
	const int32_t kind = m_kinds[index(p)];
	if (kind < 0 || (size_t)kind >= m_kindOf.size() || m_kindOf[kind] < 0)
		return true;
	const WorldMatter::Spec &s = m_blocks[m_kindOf[kind]].spec;
	return s.matter == "air" || s.leveled || s.state == matter::State::Liquid;
}

bool Stage::inside(Pos p) const
{
	return p.x >= 0 && p.y >= 0 && p.z >= 0 && p.x < m_x && p.y < m_y && p.z < m_z;
}

size_t Stage::index(Pos p) const
{
	if (!inside(p))
		throw SandboxError("a place outside the stage");
	return ((size_t)p.y * m_z + p.z) * m_x + p.x;
}

void Stage::place(Pos at, const std::string &node)
{
	const auto found = m_byName.find(node);
	if (found == m_byName.end())
		throw SandboxError("no block called " + node);
	m_kinds[index(at)] = found->second;
	m_world->disturb(at);
}

void Stage::heat(Pos at, double temperature)
{
	index(at); // (it throws outside)
	m_world->touch(at, temperature);
}

void Stage::hold(Pos at, double temperature)
{
	m_holds[index(at)] = { at, temperature };
	m_world->touch(at, temperature);
}

size_t Stage::substanceOf(const std::string &id) const
{
	const auto &all = m_matter.substances();
	for (size_t s = 0; s < all.size(); s++)
		if (all[s].id == id)
			return s;
	throw SandboxError("no substance " + id);
}

void Stage::add(Pos at, const std::string &substance, double moles, double temperature)
{
	index(at);
	m_world->add(at, substanceOf(substance), moles, temperature);
}

void Stage::pour(Pos at, const std::string &substance, double rate, double temperature, double until)
{
	index(at);
	m_pours.push_back({ at, substanceOf(substance), rate, temperature, until });
}

std::vector<std::pair<Pos, size_t>> Stage::pouring() const
{
	std::vector<std::pair<Pos, size_t>> out;
	for (const Pouring &p : m_pours)
		if (m_time < p.until)
			out.push_back({ p.at, p.substance });
	return out;
}

void Stage::setUp(const Json::Value &spec, double ambient)
{
	const auto pos = [](const Json::Value &v) -> Pos {
		if (!v.isArray() || v.size() != 3)
			throw SandboxError("a block's position is [x, y, z]");
		return { v[0].asInt(), v[1].asInt(), v[2].asInt() };
	};
	for (const Json::Value &b : spec["blocks"])
		if (b.isMember("node"))
			place(pos(b["at"]), b["node"].asString());
	for (const Json::Value &b : spec["blocks"]) {
		const Pos where = pos(b["at"]);
		const double t = b.get("temperature", ambient).asDouble();
		for (const std::string &id : b["add"].getMemberNames())
			add(where, id, b["add"][id].asDouble(), t);
		if (b.isMember("power"))
			burner(where, b["power"].asDouble());
		if (b["hold"].asBool())
			hold(where, t);
		else if (b.isMember("temperature"))
			heat(where, t);
	}
	for (const Json::Value &b : spec["blows"]) {
		const Pos where = pos(b["at"]);
		index(where);
		m_blows.push_back({ where, b["rate"].asDouble(), b["until"].asDouble() });
	}
	for (const Json::Value &p : spec["pours"])
		pour(pos(p["at"]), p["substance"].asString(), p["rate"].asDouble(), p.get("temperature", ambient).asDouble(), p["until"].asDouble());
}

void Stage::step(double seconds)
{
	const int steps = std::max(1, (int)std::ceil(seconds / LONGEST_STEP));
	for (int i = 0; i < steps; i++) {
		for (const auto &[cell, held] : m_holds)
			m_world->touch(held.first, held.second);
		const double dt = seconds / steps, now = m_time + seconds * i / steps;
		for (const auto &[at, watts] : m_burners)
			m_world->warm(at, watts * dt);
		for (const Pouring &p : m_pours)
			if (now < p.until)
				m_world->pour(p.at, p.substance, p.rate * dt, p.temperature);
		for (const Blowing &b : m_blows)
			if (now < b.until)
				m_world->blow(b.at, b.rate * dt);
		m_world->step(dt);
	}
	m_time += seconds;
}

std::vector<Stage::Cell> Stage::view() const
{
	std::vector<Cell> out((size_t)m_x * m_y * m_z);
	size_t i = 0;
	for (int y = 0; y < m_y; y++)
		for (int z = 0; z < m_z; z++)
			for (int x = 0; x < m_x; x++, i++) {
				Cell &c = out[i];
				const int32_t kind = m_kinds[i];
				c.block = kind >= 0 && (size_t)kind < m_kindOf.size() ? m_kindOf[kind] : -1;
				c.level = m_levels[i];
				c.fill = m_fills[i];
				c.fill_colour = m_colours[i];
				if (const std::optional<WorldMatter::Awake> a = m_world->inspect({ x, y, z })) {
					c.temperature = a->temperature;
					c.activity = a->activity;
					c.main = a->main;
					c.state = a->state;
					c.filled = a->fill;
					c.vapour = a->vapour;
					c.haze = a->haze;
					c.flame = a->flame;
				}
				// how near boiling its liquid is
				size_t liquid = m_matter.substances().size();
				if (c.fill.substance >= 0)
					liquid = (size_t)c.fill.substance;
				else if (c.block >= 0 && m_blocks[c.block].spec.state == matter::State::Liquid && m_blocks[c.block].spec.matter != "air")
					liquid = m_matter.index(m_blocks[c.block].spec.matter);
				if (liquid < m_matter.substances().size())
					if (const std::optional<double> boils = m_matter.substances()[liquid].boilingPoint(m_pressure))
						c.boil = std::clamp((c.temperature - (*boils - NEAR_BOILING)) / (NEAR_BOILING - 1), 0.0, 1.0);
			}
	// a solid fuel's fire is lit by its soot, whatever gas is burning over it
	uint32_t sooty = 0;
	for (const auto &[fuel, burns] : { std::pair<const char *, const char *>{ "wood", "wood_burns" }, { "carbon", "charcoal_burns" } }) {
		const int32_t id = (int32_t)m_matter.index(fuel);
		for (const Cell &c : out)
			if (!sooty && c.main == id && c.state == matter::State::Solid && c.filled > 0.005)
				for (const matter::Reaction &r : m_matter.reactions())
					if (r.id == burns)
						sooty = r.flame;
	}
	if (sooty)
		for (Cell &c : out)
			if (c.flame)
				c.flame = sooty;
	return out;
}

} // namespace sandbox
