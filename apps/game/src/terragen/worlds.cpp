// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "worlds.h"
#include "system.h"
#include <cmath>
#include <stdexcept>

namespace terragen {

// Earth's surface on its card (SystemSpec::home): what the climate's bands are
// calibrated at, so that world's push is none
static const fix EARTH_KELVIN = 19037226; // 290.49 K

// A body's processes from its card: water where it has seas, life where
// they're temperate and it arises, plates where the body model says its crust moves
Processes processesOf(const BodyCard &card, bool life)
{
	Processes p;
	p.day = card.day;
	p.facing_star = card.spin == Spin::Star;
	// (a bar of air under so sharp a contrast carries three times a spinning world's; rock alone, hardly at all)
	if (p.facing_star)
		p.carrying = clamp(3 * sqrt(std::max<fix>(card.pressure, 0)), ratio(2, 100), toFix(8));
	p.warming = clamp(card.temperature - EARTH_KELVIN, -toFix(288), toFix(3000));
	p.regime = card.plates ? Regime::Plates : Regime::Lid;
	p.water = card.ocean;
	p.life = life && card.ocean && card.kind == Kind::Temperate;
	// craters: water wears them, plates recycle the crust, volcanism buries
	// them; air burns up the small impactors (100 m a bar)
	p.craters = ONE;
	if (card.ocean) // (rain and rivers wear one away in a few million years: Earth keeps ~190)
		p.craters /= 400;
	if (card.plates)
		p.craters /= 3;
	if (card.volcanism)
		p.craters /= 4;
	p.smallest_crater = std::max<int32_t>(0, floorInt(card.pressure * 100));
	p.gravity = card.gravity;
	// maria: lava welled up through a lid for as long as the interior was
	// warm enough (until the volcanoes fell silent, or still), ¼ √Gyr of the
	// surface; the impact rate falls steeply, so few craters came after
	if (!card.plates) {
		fix lasted = card.volcanism ? toFix(4) : -1;
		for (const Moment &m : card.history)
			if (m.event == Event::VolcanoesSilent)
				lasted = m.age;
		if (lasted > 0) {
			p.flooding = std::min<fix>(HALF, sqrt(lasted) / 4);
			p.young = div(ONE, ONE + 16 * lasted);
		}
		// a mantle thick enough for plumes (not a body nearly all core), that
		// stayed warm for ages: they don't drift off a lid, so they build one
		// rise and giant shields
		// (the lava then rises through the rise, not over a hemisphere and more)
		if (lasted >= ONE && card.rock > ratio(65, 100)) {
			p.plumes = lasted;
			p.flooding = std::min<fix>(p.flooding, ratio(30, 100));
		}
	}
	// seas that froze or boiled away: their valleys remain, and their rust
	if (!card.ocean) {
		fix formed = -1;
		for (const Moment &m : card.history) {
			if (m.event == Event::OceansForm)
				formed = m.age;
			else if (formed >= 0 && (m.event == Event::OceansFreeze || m.event == Event::OceansBoil))
				p.ancient_water = m.age - formed;
		}
	}
	switch (card.kind) {
	case Kind::Temperate: p.bare = p.life ? Surface::Soil : Surface::Rock; break; // (soil is life's work)
	case Kind::ColdDesert:
	case Kind::Hothouse:
	case Kind::Hydrocarbon: p.bare = Surface::Sand; break; // (methane seas wait for the general atmosphere)
	case Kind::Volcanic: // eruptions bury everything faster than impacts mark it
		p.bare = Surface::Sulfur;
		p.eruptions = card.heat;
		p.flooding = 0;
		p.young = 0;
		p.plumes = 0;
		p.craters = div(p.craters, ONE + 16 * card.heat);
		break;
	case Kind::Airless: p.bare = Surface::Regolith; break;
	case Kind::IceShell: p.bare = Surface::Ice; break;
	case Kind::Giant:
	case Kind::Count: throw std::invalid_argument("terragen::bodyWorlds: no surface for a body of this kind");
	}
	if (p.ancient_water > 0)
		p.bare = Surface::Rust;
	// mostly ice and frozen through at the surface: the crust is ice, whatever
	// its air. Water, not lava, wells up in it, and what the tides flex keeps
	// renewing its surface, so few craters last
	if (card.ice > ratio(30, 100) && !card.ocean) {
		p.bare = Surface::Ice;
		p.stiff = ratio(40, 100);
		p.flexing = card.tidal;
		p.eruptions = 0;
		p.plumes = 0;
		p.ancient_water = 0;
		p.flooding = 0;
		p.young = 0;
		p.craters = div(p.craters, ONE + 16 * std::min<fix>(card.tidal, toFix(16)));
	}
	return p;
}

namespace {

double toDouble(fix v) { return (double)v / ONE; }

// The card's gases by the matter table's names, and their molar masses (g/mol)
const char *const GAS_MATTER[(int)Gas::Count] = { "carbon_dioxide", "nitrogen", "oxygen", "methane", "hydrogen", nullptr };
const double GAS_MOLAR[(int)Gas::Count] = { 44.01, 28.01, 32.00, 16.04, 2.016, 4.003 };

Surroundings surroundingsOf(const BodyCard &card)
{
	Surroundings s{ toDouble(card.gravity) * 9.81, std::max(0.0, toDouble(card.pressure)) * 100000, toDouble(card.temperature), {} };
	double kept = 0;
	for (int g = 0; g < (int)Gas::Count; g++)
		if (GAS_MATTER[g] && card.gases[g] > 0)
			kept += toDouble(card.gases[g]);
	if (s.pressure > 0 && kept > 0)
		for (int g = 0; g < (int)Gas::Count; g++)
			if (GAS_MATTER[g] && card.gases[g] > 0)
				s.gases.emplace_back(GAS_MATTER[g], toDouble(card.gases[g]) / kept);
	return s;
}

} // namespace

Planet planetOf(const BodyWorld &body)
{
	const Surroundings &s = body.surroundings;
	Planet p = sphere(body.atlas.radius);
	p.gravity = s.gravity;
	// ρ = P M / (R T), M the air's mean molar mass
	double molar = 0;
	for (const auto &[id, share] : s.gases)
		for (int g = 0; g < (int)Gas::Count; g++)
			if (GAS_MATTER[g] && id == GAS_MATTER[g])
				molar += share * GAS_MOLAR[g] / 1000;
	p.density = s.pressure > 0 && s.temperature > 0 ? s.pressure * molar / (8.314 * s.temperature) : 0;
	return p;
}

std::vector<BodyWorld> bodyWorlds(const WorldCode &home, const Atlas::Params &homeAtlas)
{
	const System system(SystemSpec::forWorld(home));
	const size_t playable = system.playable();
	if (system.cards()[playable].giant)
		throw std::invalid_argument("terragen::bodyWorlds: the home is a giant, with no surface");
	const double blocksPerKm = homeAtlas.radius / system.radius(playable);
	std::vector<BodyWorld> out{ { playable, system.spec().bodies[playable].id, home, homeAtlas, surroundingsOf(system.cards()[playable]) } };
	for (size_t b = 0; b < system.spec().bodies.size(); b++) {
		const double radius = system.radius(b) * blocksPerKm;
		if (b == playable || system.cards()[b].giant || radius < SMALLEST_BODY)
			continue;
		const std::string &id = system.spec().bodies[b].id;
		WorldCode code = home;
		code.processes = processesOf(system.cards()[b], system.spec().bodies[b].causes.life);
		code.tilt = (fix)std::lround(system.spec().bodies[b].tilt * ONE);
		code.setLand(system.spec().bodies[b].land.value_or(defaultLand()));
		code.text = home.text + "@" + id;
		code.seed = home.seed + "@" + id;
		code.seed_hash = home.seed_hash * 16777619u ^ hashOf(id);
		code.size = (int32_t)std::lround(2 * M_PI * radius);
		// its sky as the home's: a radius up, no higher than the home's (heights
		// are in metres since sprint 93, so 256 layers cut a moon's highlands off)
		Atlas::Params atlas = Atlas::standard(radius);
		atlas.height = (int)std::min(radius, (double)std::max(homeAtlas.height, atlas.height));
		code.keepGrid();
		out.push_back({ b, id, code, atlas, surroundingsOf(system.cards()[b]) });
	}
	return out;
}

} // namespace terragen
