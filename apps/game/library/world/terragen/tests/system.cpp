// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/tests/support.h"

// Free by default however close; a lock sets the day; a world facing its star
// has a hot side, a frozen side and a ring between to live on.
TEST_CASE("Binary")
{
	const auto with = [](Pairing kind, fix separation, int orbit_percent) {
		SystemSpec s = SystemSpec::home();
		s.star.companion = { kind, 1, ratio(75, 100), separation };
		s.bodies[1].causes.orbit = ratio(orbit_percent, 100);
		// (only Earth is the test's)
		s.bodies.resize(2);
		s.bodies.erase(s.bodies.begin());
		s.bodies[0].playable = true;
		return s;
	};
	const System one{ SystemSpec::home() };
	REQUIRE(!one.hasCompanion());

	// a close pair: their light adds, their masses both pull, they circle a point between
	const System close{ with(Pairing::Close, ratio(20, 100), 100) };
	const fix alone = luminosity(ONE), both = alone + luminosity(ratio(75, 100));
	REQUIRE(near(close.cards()[0].sunlight, both, 64));
	REQUIRE(close.cards()[0].temperature > one.cards()[1].temperature);
	REQUIRE(std::abs(close.period(0) - 365.25 / std::sqrt(1.75)) < 1);
	const System::Vec a = close.starAt(false, 3), b = close.starAt(true, 3);
	REQUIRE((std::abs(a.x * 1 + b.x * 0.75) < 1 && std::abs(a.z * 1 + b.z * 0.75) < 1));
	REQUIRE(std::abs(std::hypot(a.x - b.x, a.z - b.z) - 0.2 * 149597870.7) < 5000); // (the separation is fixed point)

	// a wide one: the first holds the planet, the other is a faint extra
	const System wide{ with(Pairing::Wide, toFix(300), 100) };
	REQUIRE(near(wide.cards()[0].sunlight, alone, 16));
	REQUIRE(std::abs(wide.period(0) - one.period(1)) < 1e-6);
	REQUIRE(wide.starAt(false, 3).x == 0);
	REQUIRE(wide.secondShare(0, 0) < 1e-3);

	// orbits that wouldn't last, stars that would touch
	REQUIRE_THROWS_AS(System{ with(Pairing::Close, ratio(50, 100), 100) }, std::invalid_argument);
	REQUIRE_THROWS_AS(System{ with(Pairing::Wide, toFix(30), 2000) }, std::invalid_argument);
	SystemSpec touching = with(Pairing::Close, ratio(2, 100), 100);
	touching.star.type = 6;
	touching.star.mass = toFix(30);
	touching.star.age = ratio(3, 1000);
	touching.star.companion.type = 6;
	touching.star.companion.mass = toFix(30);
	REQUIRE_THROWS_AS(System{ touching }, std::invalid_argument);

	// the seed's: a real companion, the same each time, whatever it lays out holding together
	int close_ones = 0, wide_ones = 0;
	for (const char *seed : { "dGVycmE", "YWJj", "c2VlZDE", "c2VlZDI", "c2VlZDM", "c2VlZDQ", "c2VlZDU", "c2VlZDY", "c2VlZDc" })
		for (int star = 0; star < STAR_COUNT; star++) {
			WorldCode c = WorldCode::decode(std::string("TF24.") + seed);
			c.style = SystemStyle::Binary;
			c.star = &STARS[star];
			c.star_mass = STARS[star].mass;
			c.age = std::min(c.age, lifetime(c.star_mass));
			const System s{ SystemSpec::forWorld(c) };
			REQUIRE(s.hasCompanion());
			(s.spec().star.companion.kind == Pairing::Close ? close_ones : wide_ones)++;
			REQUIRE(SystemSpec::companionOf(c).mass == s.spec().star.companion.mass);
		}
	REQUIRE((close_ones > 0 && wide_ones > 0));
}

TEST_CASE("Double")
{
	// our Moon's balance point is inside Earth: no double
	const System usual{ SystemSpec::home() };
	REQUIRE((!usual.partner(1) && !usual.partner(4)));

	// a second Earth ten radii off
	const auto doubled = [](Spin both) {
		SystemSpec s = SystemSpec::home();
		SystemBody other = s.bodies[1];
		other.id = "other";
		other.playable = false;
		other.causes.parent = 1;
		other.causes.orbit = toFix(10);
		other.causes.eccentricity = 0;
		other.causes.spin = both;
		other.phase = 0.3;
		s.bodies[1].causes.spin = both;
		s.bodies[1].causes.eccentricity = 0;
		s.bodies.push_back(other);
		return s;
	};
	const System free{ doubled(Spin::Free) };
	const size_t other = free.spec().bodies.size() - 1;
	REQUIRE((free.partner(1) == other && free.partner(other) == (size_t)1));
	// they're opposite each other round a point between them, which keeps the star's orbit
	const System::Vec a = free.position(1, 40), b = free.position(other, 40), mid = free.balance(1, 40);
	const double moon = free.spec().bodies[4].causes.mass / 65536.0; // (the Moon swings Earth a little too)
	const double apart = std::hypot(a.x - b.x, a.y - b.y, a.z - b.z);
	REQUIRE(std::abs(apart - 10 * free.radius(1)) < free.radius(1) / 100);
	const System::Vec r = free.fromHost(other, 40), m = free.fromHost(4, 40);
	REQUIRE(std::abs((a.x + r.x / 2 + m.x * moon / (1 + moon)) - mid.x) < 1);
	REQUIRE(std::abs((b.x - r.x / 2 + m.x * moon / (1 + moon)) - mid.x) < 1);
	// both masses pull: √2 quicker than the same orbit round one alone
	const double a_km = 10 * free.radius(1);
	REQUIRE(std::abs(free.period(other) - 2 * M_PI * std::sqrt(a_km * a_km * a_km / (398600.4418 * 2)) / 86400) < 0.01);
	// each flexes the other
	REQUIRE((free.cards()[1].tidal > 0 && free.cards()[1].tidal == free.cards()[other].tidal));

	// held to each other: each stays fixed in the other's sky, through the compressed calendar and between days
	const System held{ doubled(Spin::Partner) };
	const double month = held.period(other) * 24;
	REQUIRE(std::abs(held.cards()[1].day / 65536.0 - month) < month / 100);
	REQUIRE(std::abs(held.cards()[other].day / 65536.0 - month) < month / 100);
	REQUIRE(held.spec().bodies[1].tilt == 0);
	const auto seen = [&](const System &s, size_t from, size_t to, double worldDays) {
		const double day = s.dayFor(worldDays, 48), time = worldDays - std::floor(worldDays);
		const System::Vec p = s.position(from, day), q = s.position(to, day);
		const double turned = std::atan2(-p.z, -p.x) + 2 * M_PI * (s.clock(from, worldDays, 48).hour(time) - 0.5);
		return std::atan2(q.z - p.z, q.x - p.x) - turned;
	};
	for (double d : { 3.7, 10.1, 31.5 }) {
		REQUIRE(std::abs(std::remainder(seen(held, other, 1, d) - seen(held, other, 1, 3.2), 2 * M_PI)) < 0.02);
		REQUIRE(std::abs(std::remainder(seen(held, 1, other, d) - seen(held, 1, other, 3.2), 2 * M_PI)) < 0.02);
	}
	// spinning free, it crosses the sky
	REQUIRE(std::abs(std::remainder(seen(free, 1, other, 3.45) - seen(free, 1, other, 3.2), 2 * M_PI)) > 0.5);
	// the lock reaches the code, and starts on either half
	const WorldCode code = WorldCode::decode(codeWith(doubled(Spin::Partner)));
	REQUIRE((code.processes.day > toFix(24) && code.tilt == 0));

	// what can't hold
	SystemSpec close = doubled(Spin::Free);
	close.bodies.back().causes.orbit = ratio(19, 10);
	REQUIRE_THROWS_AS(System{ close }, std::invalid_argument);
	// (a body many times lighter but puffed up with ice, torn apart outside where it would touch)
	SystemSpec torn = SystemSpec::home();
	torn.bodies[4].causes.orbit = ratio(150, 100);
	torn.bodies[4].causes.mass = ratio(1, 100);
	torn.bodies[4].causes.formed = ratio(90, 100);
	REQUIRE_THROWS_AS(System{ torn }, std::invalid_argument);

	// everything the seed lays out holds together, doubles too
	for (const char *seed : { "dGVycmE", "YWJj", "c2VlZDE", "c2VlZDI", "c2VlZDM", "c2VlZDQ", "c2VlZDU" }) {
		WorldCode c = WorldCode::decode(std::string("TF24.") + seed);
		for (int style = (int)SystemStyle::SunLike; style < (int)SystemStyle::Count; style++) {
			c.style = (SystemStyle)style;
			const System s{ SystemSpec::generate(c) };
			if (c.style == SystemStyle::Double)
				REQUIRE(s.partner(s.playable()).has_value());
		}
	}
}
