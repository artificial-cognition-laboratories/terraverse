// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/tests/support.h"

// As the page's encodeWorldCode writes them: an Earth at 2 AU, a moon called Mún.
static const char *const PAGE_BODIES = "TF24.YWJj.AAKAqoCAAG1mgICAgICAgICCVQ.AgVlYXJ0aAVFYXJ0aFCkxf8DaAkAACIrA1mZgAMuAW0ETcO6bqq7zAAAkTeAwdFWDU0aGvsO";

TEST_CASE("Spin")
{
	const System usual{ SystemSpec::home() };
	for (const BodyCard &c : usual.cards())
		REQUIRE(!c.locked);
	REQUIRE((usual.clock(1, 12.25, 48).turns == 1 && usual.clock(1, 12.25, 48).at == 0)); // Earth's clock is the engine's

	// the Moon held to Earth: its day is its month, the same face to Earth for good
	SystemSpec held = SystemSpec::home();
	held.bodies[4].causes.spin = Spin::Partner;
	held.bodies[4].tilt = 7;
	const System moon{ held };
	REQUIRE((moon.cards()[4].locked && moon.spec().bodies[4].tilt == 0));
	const double month = moon.period(4) * 24;
	REQUIRE(std::abs(moon.cards()[4].day / 65536.0 - month) < month / 100);
	REQUIRE(std::abs(std::remainder(moon.turning(4, 3.7) - 2 * M_PI * (moon.spec().bodies[4].phase + 3.7 / moon.period(4)), 2 * M_PI)) < 1e-9);

	// three turns in two orbits: Mercury's, whose day is two of its years
	SystemSpec mercury = SystemSpec::home();
	mercury.bodies[0].causes.spin = Spin::Res3_2;
	const System m{ mercury };
	REQUIRE(std::abs(m.cards()[0].day / 65536.0 - m.period(0) * 24 / 1.5) < m.period(0) * 24 / 100);
	// (the playable body's year is Earth's: Cinder's orbits as the world's calendar compresses them)
	const double perWorldDay = m.period(1) / 48;
	REQUIRE(std::abs(m.clock(0, 0, 48).turns - 0.5 / m.period(0) * perWorldDay) < 1e-9);
	REQUIRE(std::abs(std::fmod(m.clock(0, 5, 48).hour(0.5) - m.clock(0, 4, 48).hour(0.5), 1.0) - std::fmod(m.clock(0, 0, 48).turns, 1.0)) < 1e-6);

	// what can't hold
	SystemSpec bad = SystemSpec::home();
	bad.bodies[4].causes.spin = Spin::Star;
	REQUIRE_THROWS_AS(System{ bad }, std::invalid_argument);
	bad = SystemSpec::home();
	bad.bodies[1].causes.spin = Spin::Partner;
	REQUIRE_THROWS_AS(System{ bad }, std::invalid_argument);

	// Earth facing the Sun
	SystemSpec facing = SystemSpec::home();
	facing.bodies[1].causes.spin = Spin::Star;
	const WorldCode code = WorldCode::decode(codeWith(facing));
	REQUIRE((code.processes.facing_star && code.tilt == 0));
	REQUIRE(code.processes.day > toFix(8000));
	const World w(code);
	const auto at = [&](int degrees) { // on the equator, from under the star
		const double a = degrees * M_PI / 180;
		return Place(normalise((int64_t)(std::cos(a) * ONE), 0, (int64_t)(std::sin(a) * ONE)));
	};
	const fix hot = yearly(w, at(0)), ring = yearly(w, at(90)), night = yearly(w, at(180));
	REQUIRE(hot > toFix(45));
	REQUIRE(night < -toFix(25));
	REQUIRE((ring > -toFix(15) && ring < toFix(30)));
	REQUIRE((yearly(w, at(80)) > 0 && yearly(w, at(80)) < toFix(30))); // (where a new player starts)
	// no seasons, no day's swing: the sun never moves
	REQUIRE(w.temperature(at(90), ratio(1, 4), ratio(1, 8)) == w.temperature(at(90), ratio(3, 4), ratio(5, 8)));
	// the zones are round the star, not the pole: both poles lie on the ring
	REQUIRE(std::abs((double)(yearly(w, Place(Dir{ 0, ONE, 0 })) - ring)) < 8 * ONE);
}

TEST_CASE("HomeFollowsCard")
{
	// the default world is Earth's, with nothing pushing its climate
	const WorldCode earth = WorldCode::decode("TF24.dGVycmE");
	REQUIRE(earth.processes.warming == 0);
	REQUIRE((earth.processes.water && earth.processes.life && earth.processes.regime == Regime::Plates));

	const Place tropic = dirAt(ratio(20, 100), toFix(1));
	const fix warm = yearly(World(earth), tropic);
	REQUIRE((warm > toFix(10) && warm < toFix(40)));

	// moved out to 5 AU: its card freezes, so does its land
	SystemSpec far = SystemSpec::home();
	far.bodies[1].causes.orbit = toFix(5);
	const WorldCode frozen = WorldCode::decode(codeWith(far));
	REQUIRE((!frozen.processes.water && !frozen.processes.life));
	REQUIRE(frozen.processes.warming < -toFix(30)); // (carbon dioxide builds up as the seas freeze: the card keeps it at 251 K)
	const World cold(frozen);
	REQUIRE(yearly(cold, tropic) < 0);
	REQUIRE(cold.ground(tropic, cold.sunlight(tropic, std::nullopt)).kind == Water::None);

	// the body's tilt is the world's, and moving a moon keeps the land
	SystemSpec tipped = SystemSpec::home();
	tipped.bodies[1].tilt = 40;
	REQUIRE(roundInt(WorldCode::decode(codeWith(tipped)).tilt) == 40);
	SystemSpec moved = SystemSpec::home(), moved2 = moved;
	moved.bodies[4].phase = 0.2;
	moved2.bodies[4].phase = 0.7;
	REQUIRE_EQ(std::string(WorldCode::decode(codeWith(moved)).grid_key), std::string(WorldCode::decode(codeWith(moved2)).grid_key));

	// starting on the Moon: a dry, airless lid; Earth still there to visit; its year is Earth's
	SystemSpec moon = SystemSpec::home();
	moon.bodies[1].playable = false;
	moon.bodies[4].playable = true;
	const WorldCode there = WorldCode::decode(codeWith(moon));
	REQUIRE((!there.processes.water && !there.processes.life));
	REQUIRE((there.processes.regime == Regime::Lid && there.processes.bare == Surface::Regolith));
	const std::vector<BodyWorld> worlds = bodyWorlds(there, Atlas::standard(there.size / (2 * M_PI)));
	REQUIRE_EQ(std::string(worlds[0].id), "moon");
	REQUIRE(std::any_of(worlds.begin(), worlds.end(), [](const BodyWorld &b) { return b.id == "earth"; }));
	REQUIRE(worlds[0].surroundings.pressure == 0);
	const System sys(SystemSpec::forWorld(there));
	REQUIRE(std::abs(sys.year() - 365.25) < 1);

	// a giant can't be stood on: refused, not built
	SystemSpec giant = SystemSpec::home();
	giant.bodies[1].playable = false;
	giant.bodies[3].playable = true;
	REQUIRE_THROWS_AS(WorldCode::decode(codeWith(giant)), CodeError);

	// a sun-like seed round each ordinary star is somewhere to live
	for (int kind : { 1, 2, 3 }) {
		int living = 0;
		for (int seed = 0; seed < 50; seed++) {
			WorldCode c = WorldCode::decode("TF24.dGVycmE");
			c.seed_hash = seed * 2654435761u;
			c.style = SystemStyle::SunLike;
			c.star = &STARS[kind];
			c.star_mass = STARS[kind].mass;
			c.age = std::min(c.age, lifetime(c.star_mass));
			const System s{ SystemSpec::forWorld(c) };
			const BodyCard &home = s.cards()[s.playable()];
			if (home.ocean && home.kind == Kind::Temperate)
				living++;
		}
		note() << "sun-like homes round " << STARS[kind].id << ": " << living << "/50 live" << std::endl;
		REQUIRE(living >= 45);
	}
}

TEST_CASE("EveryBodysLand")
{
	const auto moonOf = [](const SystemSpec &s) {
		const WorldCode home = WorldCode::decode(codeWith(s));
		for (const BodyWorld &b : bodyWorlds(home, Atlas::standard(250000)))
			if (b.id == "moon")
				return b.code;
		throw std::logic_error("no moon to stand on");
	};
	const WorldCode usual = moonOf(SystemSpec::home());
	REQUIRE(usual.mountains == WorldCode::decode("TF24.dGVycmE").mountains);

	// a slider reaches the moon's own land, and nothing else's
	LandBytes high = defaultLand();
	high[3] = 255; // mountains
	high[4] = 255; // roughness
	high[6] = 0;   // humidity
	SystemSpec set = SystemSpec::home();
	set.bodies[4].land = high;
	const WorldCode rough = moonOf(set);
	REQUIRE(rough.mountains == ONE);
	REQUIRE(rough.grid_key != usual.grid_key);
	REQUIRE_EQ(std::string(WorldCode::decode(codeWith(set)).grid_key), std::string(WorldCode::decode(codeWith(SystemSpec::home())).grid_key));
	const World flat(usual), tall(rough);
	int differ = 0;
	for (int i = 0; i < 64; i++) {
		const Place at = dirAt(ratio(i - 32, 40), ratio(i * 5, 8));
		if (flat.ground(at, flat.sunlight(at, std::nullopt)).terrain != tall.ground(at, tall.sunlight(at, std::nullopt)).terrain)
			differ++;
	}
	REQUIRE(differ > 32);

	// the code's land settings are the start body's: they don't reach the moon
	WorldCode home = WorldCode::decode(codeWith(SystemSpec::home()));
	home.mountains = 0;
	REQUIRE(bodyWorlds(home, Atlas::standard(250000))[1].code.mountains != 0);

	// another body's edit moves no moon's land; its grid is kept by what decides it
	SystemSpec nudged = SystemSpec::home();
	nudged.bodies[0].phase = 0.9;
	nudged.bodies[3].inclination = 3;
	REQUIRE_EQ(std::string(moonOf(nudged).grid_key), std::string(usual.grid_key));
}

TEST_CASE("Stars" * doctest::test_suite("slow"))
{
	REQUIRE(luminosity(ONE) == ONE);
	REQUIRE(luminosity(toFix(30)) > toFix(10000));
	REQUIRE(luminosity(ratio(30, 100)) < ratio(2, 100));
	REQUIRE((lifetime(ONE) == toFix(10) || near(lifetime(ONE), toFix(10), 64)));
	REQUIRE(lifetime(toFix(30)) < ratio(1, 100));

	const std::vector<BodyCauses> earth = { { -1, real(1), real(0.017), real(1), real(0.35), real(0.6), real(24), true } };
	const BodyCard ours = bodies({ 2, ONE, 0, real(4.5), real(0.5) }, earth)[0];
	REQUIRE_EQ(std::string(KIND_NAMES[(int)ours.kind]), std::string(KIND_NAMES[(int)Kind::Temperate]));

	// an O star's world: a few million years old and baking, however close, at any distance in range
	const BodyCard baked = bodies({ 6, toFix(30), 0, ratio(3, 1000), real(0.5) }, earth)[0];
	REQUIRE(baked.temperature > toFix(1000));
	REQUIRE(!baked.ocean);
	const BodyCauses close = { -1, real(0.05), 0, real(1), real(0.35), real(0.6), real(24), false };
	const BodyCard burnt = bodies({ 6, toFix(60), 0, ratio(3, 1000), real(0.5) }, { close })[0];
	REQUIRE(burnt.temperature > toFix(1000));

	const BodyCard poor = bodies({ 2, ONE, -ONE, real(4.5), real(0.5) }, earth)[0];
	REQUIRE(poor.metal < ours.metal);

	// as the page encodes them (ui/src/worldgen/config.ts): the star's whole description
	const WorldCode oh = WorldCode::decode("TF24.YWJj.AQa1VQVAAG1mgICAgICAgICCVQ");
	REQUIRE_EQ(std::string(oh.star->id), "O");
	REQUIRE(near(oh.star_mass, toFix(40), ONE / 8));
	REQUIRE(oh.metallicity == -HALF);
	REQUIRE((oh.age >= ratio(2, 1000) && oh.age <= lifetime(oh.star_mass))); // however it rounds
	REQUIRE(oh.activity == ONE / 4);
	System{ SystemSpec::forWorld(oh) };

	// a system from the seed: its own, the same every time, in the model's range
	WorldCode a = WorldCode::decode("TF24.YWJj.AAKAqoCAAW1mgICAgICAgICCVQ");
	WorldCode b = a;
	b.seed_hash ^= 77;
	const SystemSpec one = SystemSpec::generate(a), again = SystemSpec::generate(a), other = SystemSpec::generate(b);
	REQUIRE(one.bodies.size() == again.bodies.size());
	for (size_t i = 0; i < one.bodies.size(); i++) {
		REQUIRE_EQ(std::string(one.bodies[i].name), std::string(again.bodies[i].name));
		REQUIRE(one.bodies[i].causes.orbit == again.bodies[i].causes.orbit);
	}
	bool differs = one.bodies.size() != other.bodies.size();
	for (size_t i = 0; !differs && i < one.bodies.size(); i++)
		differs = one.bodies[i].causes.orbit != other.bodies[i].causes.orbit;
	REQUIRE(differs);
	// any star, any style, any seed: it can exist, and the home has ground under it
	for (uint32_t seed = 0; seed < 200; seed++)
		for (int style = (int)SystemStyle::SunLike; style < (int)SystemStyle::Count; style++)
			for (int kind = 0; kind < STAR_COUNT; kind++) {
				WorldCode c = a;
				c.seed_hash = seed * 2654435761u;
				c.style = (SystemStyle)style;
				c.star = &STARS[kind];
				c.star_mass = STARS[kind].mass;
				c.age = std::min(c.age, lifetime(c.star_mass));
				const System made{ SystemSpec::forWorld(c) };
				REQUIRE(!made.cards()[made.playable()].giant);
			}

	// the page's bodies (bodyBytes): one planet, one moon
	const SystemSpec read = SystemSpec::forWorld(WorldCode::decode(PAGE_BODIES));
	REQUIRE(read.bodies.size() == 2);
	REQUIRE_EQ(std::string(read.bodies[1].name), "Mún");
	REQUIRE(read.bodies[1].causes.parent == 0);
	REQUIRE(near(read.bodies[0].causes.orbit, toFix(2), 64));
	REQUIRE(near(read.bodies[1].causes.mass, ratio(1, 100), 8));
	REQUIRE(read.bodies[0].playable);
	System{ read };
	REQUIRE_THROWS_AS(SystemSpec::read({ 1, 9 }), CodeError);

	// a star can't have outlived itself
	REQUIRE_THROWS_AS(bodies({ 6, toFix(30), 0, toFix(4), real(0.5) }, earth), std::invalid_argument);
}

// Our solar system from causes alone: every body must come out as itself.
TEST_CASE("Bodies")
{
	StarCauses sun{ 2, ONE, 0, real(4.5), real(0.5) };
	std::vector<BodyCauses> c = {
		{ -1, real(1), real(0.017), real(1), real(0.35), real(0.6), real(24), true },          // Earth
		{ -1, real(0.72), real(0.007), real(0.82), real(0.3), real(0.6), real(5832), false },   // Venus
		{ -1, real(1.52), real(0.09), real(0.107), real(0.45), real(0.5), real(24.6), false },  // Mars
		{ -1, real(0.39), real(0.2), real(0.055), real(0.1), real(0.05), real(1408), false },   // Mercury
		{ 0, real(60), real(0.055), real(0.0123), real(0.35), real(0.05), real(655), false },   // the Moon
		{ -1, real(5.2), real(0.05), real(318), real(0.8), real(0.6), real(10), false },        // Jupiter
		{ 5, real(5.9), real(0.0041), real(0.015), real(0.55), real(0.05), real(42), false },   // Io
		{ 5, real(9.4), real(0.009), real(0.008), real(0.8), real(0.6), real(85), false },      // Europa
		{ -1, real(9.5), real(0.05), real(95), real(0.85), real(0.6), real(10.7), false },      // Saturn
		{ 8, real(20), real(0.029), real(0.0225), real(0.85), real(0.7), real(383), false },    // Titan
	};
	std::vector<BodyCard> b = bodies(sun, c);
	const Kind kinds[] = { Kind::Temperate, Kind::Hothouse, Kind::ColdDesert, Kind::Airless, Kind::Airless,
		Kind::Giant, Kind::Volcanic, Kind::IceShell, Kind::Giant, Kind::Hydrocarbon };
	for (size_t i = 0; i < b.size(); i++)
		REQUIRE(b[i].kind == kinds[i]);

	const BodyCard &earth = b[0];
	REQUIRE((earth.pressure > real(0.7) && earth.pressure < real(1.6)));
	REQUIRE((earth.temperature > toFix(278) && earth.temperature < toFix(300)));
	REQUIRE((earth.gases[(int)Gas::N2] > real(0.7) && earth.gases[(int)Gas::O2] > real(0.1)));
	REQUIRE((earth.field > HALF && earth.ocean && earth.plates));
	REQUIRE(b[1].pressure > toFix(5));                      // Venus: thick CO₂
	REQUIRE(b[1].gases[(int)Gas::CO2] > real(0.9));
	REQUIRE(b[6].tidal > toFix(10));        // the Moon faces us; Io is tidally heated

	// insides: Earth's molten core is about half its radius; Europa hides an
	// ocean under its ice; Jupiter is mostly envelope
	auto layer = [](const BodyCard &card, const char *name) -> const Layer * {
		for (const Layer &l : card.layers)
			if (std::string(l.name) == name)
				return &l;
		return nullptr;
	};
	REQUIRE((layer(earth, "core") && layer(earth, "core")->molten));
	REQUIRE((layer(earth, "core")->outer > real(0.45) && layer(earth, "core")->outer < real(0.65)));
	REQUIRE((layer(earth, "ocean") && !layer(b[2], "ocean")));
	REQUIRE((layer(b[7], "ocean") && layer(b[7], "ice") && layer(b[7], "ocean")->outer < layer(b[7], "ice")->outer));
	REQUIRE((layer(b[5], "envelope") && !layer(b[5], "crust")));

	auto told = [](const BodyCard &card, Event e) {
		for (const Moment &m : card.history)
			if (m.event == e)
				return true;
		return false;
	};
	REQUIRE((told(b[2], Event::FieldFades) && told(b[2], Event::OceansFreeze))); // Mars's story
	REQUIRE((told(b[1], Event::Runaway) && !told(b[3], Event::Runaway)));
	REQUIRE((told(earth, Event::LifeAppears) && told(earth, Event::OxygenRises)));

	// the same causes, the same cards
	std::vector<BodyCard> again = bodies(sun, c);
	for (size_t i = 0; i < b.size(); i++)
		REQUIRE((again[i].temperature == b[i].temperature && again[i].pressure == b[i].pressure));

	// impossible systems are refused
	std::vector<BodyCauses> bad = { c[4] }; // a moon with no planet before it
	REQUIRE_THROWS_AS(bodies(sun, bad), std::invalid_argument);
}
