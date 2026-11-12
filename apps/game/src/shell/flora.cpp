// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "flora.h"
#include "bridge.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <optional>

namespace shell {

static int lineageCount() { return (int)flora::presets().size(); } // of a world: one of each preset, till worlds lay out their own
static const int GROUND = 12;   // blocks out from the plant's foot, each way
static double real(terragen::fix v) { return v / (double)terragen::ONE; }

static terragen::fix fixed(const Json::Value &site, const char *field, double lo, double hi)
{
	if (!site[field].isNumeric())
		throw Refused(std::string("flora: a site's '") + field + "' is a number");
	const double v = site[field].asDouble();
	if (!(v >= lo && v <= hi))
		throw Refused(std::string("flora: a site's '") + field + "' is out of range");
	return (terragen::fix)std::lround(v * terragen::ONE);
}

static u32 hashOf(const std::string &text)
{
	u32 h = 2166136261u; // FNV-1a
	for (const unsigned char c : text)
		h = (h ^ c) * 16777619u;
	return h;
}

static std::string hex(u32 rgb)
{
	char out[8];
	snprintf(out, sizeof(out), "#%06x", rgb & 0xffffff);
	return out;
}

Flora::Flora()
{
	regrow();
}

void Flora::set(const Json::Value &intent)
{
	const Json::Value &site = intent["site"];
	if (!intent["world"].isString() || !intent["lineage"].isInt() || !intent["plant"].isInt() || !site.isObject() || !site["bank"].isBool() || !site["depth"].isInt())
		throw Refused("flora.set takes { world, lineage, plant, site }");
	const int chosen = intent["lineage"].asInt();
	if (chosen < 0 || chosen >= lineageCount())
		throw Refused("flora: no such lineage");
	flora::Site s;
	s.heat = fixed(site, "heat", -1, 1);
	s.wet = fixed(site, "wet", -1, 1);
	s.season = fixed(site, "season", -1, 1);
	s.fertility = fixed(site, "fertility", 0, 1);
	s.sand = fixed(site, "sand", 0, 1);
	s.clay = fixed(site, "clay", 0, 1);
	s.stones = fixed(site, "stones", 0, 1);
	s.wind = fixed(site, "wind", 0, 1);
	s.slope = fixed(site, "slope", 0, 4);
	s.upset = fixed(site, "upset", 0, 1);
	s.light = fixed(site, "light", 0, 1);
	s.depth = site["depth"].asInt();
	if (s.depth < 0 || s.depth > 64)
		throw Refused("flora: a site's 'depth' is out of range");
	s.bank = site["bank"].asBool();
	const Json::Value &life = intent["life"];
	if (!life.isObject())
		throw Refused("flora.set takes a life: { age, season }");
	m_life.age = fixed(life, "age", 0, 1);
	m_life.season = fixed(life, "season", 0, 1);
	// genes set by hand, over the lineage's: by id
	m_genes.clear();
	const Json::Value &genes = intent["genes"];
	if (!genes.isNull() && !genes.isObject())
		throw Refused("flora: genes are { id: value }");
	for (int g = 0; g < (int)flora::Gene::Count; g++) {
		const flora::GeneInfo &info = flora::GENES[g];
		if (!genes.isMember(info.id))
			continue;
		const double lo = real(info.lo), hi = real(info.hi);
		m_genes[g] = fixed(genes, info.id, lo, hi);
	}
	m_world = intent["world"].asString();
	m_chosen = chosen;
	m_seed = (u32)intent["plant"].asInt();
	m_site = s;
	regrow();
}

void Flora::regrow()
{
	m_lineages = flora::lineages(hashOf(m_world), lineageCount());
	flora::Lineage l = m_lineages.at(m_chosen);
	for (const auto &[g, v] : m_genes)
		l.genes[g] = v;
	m_traits = flora::traits(l, m_site, m_life);
	m_plant = flora::grow(m_traits, l.seed ^ (m_seed * 0x9e3779b9u));
	m_version++;
}

Json::Value Flora::state() const
{
	Json::Value out;
	Json::Value &list = out["lineages"] = Json::Value(Json::arrayValue);
	for (const flora::Lineage &l : m_lineages) {
		const flora::Preset &p = flora::presets().at(l.preset);
		Json::Value row;
		row["name"] = l.name;
		row["preset"] = p.label;
		row["group"] = p.group;
		row["leaf"] = hex(l.colours.leaf);
		row["bark"] = hex(l.colours.bark);
		row["lives"] = flora::traits(l, m_site, m_life).lives;
		list.append(row);
	}
	// the chosen lineage's genes: what it was born with (or set by hand), and what this site made of them
	const flora::Lineage &l = m_lineages.at(m_chosen);
	Json::Value &genes = out["genes"] = Json::Value(Json::arrayValue);
	for (int g = 0; g < (int)flora::Gene::Count; g++) {
		const flora::GeneInfo &info = flora::GENES[g];
		Json::Value row;
		row["id"] = info.id;
		row["label"] = info.label;
		row["group"] = info.group;
		row["lo"] = real(info.lo);
		row["hi"] = real(info.hi);
		row["whole"] = info.whole;
		const auto set = m_genes.find(g);
		row["value"] = real(set != m_genes.end() ? set->second : l.genes[g]);
		row["grown"] = real(m_traits.genes[g]);
		row["set"] = set != m_genes.end();
		genes.append(row);
	}
	const flora::Strategy s = flora::strategy(m_site);
	Json::Value &st = out["strategy"];
	st["warmth"] = real(s.warmth);
	st["water"] = real(s.water);
	st["light"] = real(s.light);
	st["food"] = real(s.food);
	st["rooting"] = real(s.rooting);
	st["vigour"] = real(s.vigour);
	st["disturbance"] = real(s.disturbance);
	st["strategy"] = flora::KIND_NAMES[(int)s.kind]; // (never a field named `kind`: the bridge's)
	st["limit"] = flora::LIMIT_NAMES[(int)s.limit];
	const flora::Traits &t = m_traits;
	Json::Value &tr = out["traits"];
	tr["lives"] = t.lives;
	tr["why"] = flora::LIMIT_NAMES[(int)t.why];
	tr["evergreen"] = t.evergreen;
	tr["leafing"] = real(t.leafing);
	tr["flowering"] = real(t.flowering);
	tr["leaf"] = hex(t.colours.leaf);
	const double share = 1.0 / (m_plant.grain * m_plant.grain * m_plant.grain);
	Json::Value &b = out["blocks"];
	for (int part = 0; part < (int)flora::Part::Count; part++)
		b[flora::PART_NAMES[part]] = m_plant.count[part] * share;
	out["grain"] = m_plant.grain;
	return out;
}

static video::SColor colourOf(u32 rgb)
{
	return video::SColor(255, (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
}

Scene::Boxes Flora::boxes() const
{
	Scene::Boxes out;
	out.version = m_version;
	const flora::Colours &c = m_traits.colours;
	// green stems take the leaf's colour, woody ones the bark's; half-woody (a palm's, a tree fern's) between
	const bool barked = m_traits.gene(flora::Gene::Woody) >= flora::ratio(25, 100);
	// by Part: wood, branch, leaf, flesh, flower, vein
	for (const u32 rgb : { c.bark, c.bark, c.leaf, barked ? c.bark : c.leaf, c.bloom, c.vein })
		out.looks.push_back({ {}, false, colourOf(rgb) });
	const float side = 1.f / m_plant.grain;
	// each column's run of one part is one box (they come sorted by y)
	struct Run { int from, to; };
	std::map<std::array<int, 4>, std::vector<Run>> columns;
	for (const flora::Voxel &v : m_plant.voxels) {
		std::vector<Run> &runs = columns[{ (int)v.part, v.x, v.z, v.part == flora::Part::Branch ? v.size : 16 }];
		if (!runs.empty() && runs.back().to == v.y - 1)
			runs.back().to = v.y;
		else
			runs.push_back({ v.y, v.y });
	}
	for (const auto &[column, runs] : columns)
		for (const Run &r : runs) {
			const float thick = column[3] / 16.f * side, tall = (r.to - r.from + 1) * side;
			out.boxes.push_back({ v3f(column[1] * side, r.from * side + tall / 2, column[2] * side), v3f(thick, tall, thick), column[0] });
		}
	return out;
}

// The site's ground, flat: its soil's colour, greener the more grows on it.
Scene::Ground Flora::ground() const
{
	const flora::Strategy s = flora::strategy(m_site);
	const double sand = real(m_site.sand), clay = real(m_site.clay), rich = real(m_site.fertility);
	const double cover = std::clamp(real(s.vigour) * 1.6, 0.0, 1.0), water = real(s.water);
	const double soil[3] = { 92 + 110 * sand + 40 * clay - 40 * rich, 74 + 95 * sand - 30 * rich, 56 + 60 * sand - 25 * clay - 25 * rich };
	const double grass[3] = { 150 - 70 * water, 150 - 10 * water, 80 - 20 * water };
	Scene::Ground g;
	g.version = m_version;
	g.width = g.depth = 2 * GROUND + 1;
	g.corner = v3f(-GROUND - 0.5f, 0, -GROUND - 0.5f);
	g.cell = g.rise = 1;
	g.floor = -1;
	g.top.assign((size_t)g.width * g.depth, 0);
	for (int z = 0; z < g.depth; z++)
		for (int x = 0; x < g.width; x++) {
			u32 h = (u32)x * 0x27d4eb2du ^ (u32)z * 0x165667b1u;
			h = (h ^ (h >> 15)) * 0x2c1b3c6du;
			const double shade = 0.94 + ((h >> 8) & 255) / 255.0 * 0.12;
			u32 c = 0;
			for (int i = 0; i < 3; i++)
				c = (c << 8) | (u32)std::clamp((soil[i] + (grass[i] - soil[i]) * cover) * shade, 0.0, 255.0);
			g.colours.push_back(c);
		}
	return g;
}

float Flora::distance() const
{
	return std::max(2.f * m_plant.top, 3.f * m_plant.reach) / m_plant.grain + 2.5f;
}

} // namespace shell
