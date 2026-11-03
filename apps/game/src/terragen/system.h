// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "body.h"
#include "code.h"
#include <optional>
#include <string>
#include <vector>

/*
	A planetary system: what a new world is (planning/sprint-35-system.md).
	The star and every body round it as the player made it (causes only),
	what each body is today (its card, from the body model: body.h), and
	where each is on its orbit at a moment. The menu, the create screen and
	the game all read this one model; the page edits causes, never computes
	a body.

	Orbits are Kepler's, round the star for a planet and round its parent
	for a moon, their periods from the masses, in real units (km, days).
	They're doubles: a position places a body in a view, it never decides a
	block (the causes and cards, which can, are fixed point).
*/
namespace terragen {

struct SystemBody
{
	std::string id, name;
	std::string colour; // "#rrggbb": its swatch in lists (the page's, as it was designed)
	BodyCauses causes;
	double phase;       // 0..1: where on its orbit it is at day 0
	double inclination; // degrees: its orbit's tilt from its parent's plane (the star's: the ecliptic)
	double tilt;        // degrees: its spin axis's from its orbit's
	bool playable;      // the body the world is played on (one per system)
	std::optional<LandBytes> land; // its own land settings; none: the usual. The start body's are the code's own.
};

// How many bodies a system can have; the page's LIMITS.bodies.
constexpr size_t MOST_BODIES = 32;

struct SystemSpec
{
	StarCauses star;
	std::vector<SystemBody> bodies; // a planet before its moons

	// The starter system: like ours, so the model's worlds are
	// recognisable. A Mercury, an Earth and Moon, a small Mars with a moon,
	// a Jupiter with four.
	static SystemSpec home();
	// A system the seed lays out in a style, round the code's star: the home
	// planet near where its star's warmth suits it, small worlds inside the
	// frost line, giants past it, moons by what can hold them. Fixed point
	// throughout. Changing what it makes for a seed needs a new code version.
	static SystemSpec generate(const WorldCode &code);
	// The code's bodies as the page wrote them (ui/src/worldgen/config.ts
	// bodyBytes). Throws CodeError if they can't be read.
	static SystemSpec read(const std::vector<uint8_t> &bytes);
	// A world's system: the code's own bodies, or its style's, round its star,
	// its playable body tilted as the code says unless its bodies say.
	static SystemSpec forWorld(const WorldCode &code);
	// A world's second star: the code's, or under style Binary the seed's. Fixed
	// point; close if there's room inside the home's warm orbit for planets, else wide.
	static Companion companionOf(const WorldCode &code);
};

class System
{
public:
	struct Vec
	{
		double x, y, z;
	};

	// Throws std::invalid_argument for a system that can't exist (the body
	// model's checks; none or several playable bodies).
	explicit System(SystemSpec spec);

	const SystemSpec &spec() const { return m_spec; }
	const std::vector<BodyCard> &cards() const { return m_cards; }
	size_t playable() const { return m_playable; }

	// Where a body is `days` after day 0, km from the star: the ecliptic is
	// x–z, y its north.
	Vec position(size_t body, double days) const;
	double radius(size_t body) const; // km (a giant's to its cloud tops)
	double period(size_t body) const; // days, round what it orbits (a moon and its planet: both masses pulling)
	// The other half of a double planet (body.h partnerOf), if it's in one.
	std::optional<size_t> partner(size_t body) const;
	// What a moon's orbit does for its planet: the planet's share of the pair's
	// mass, the part of their distance the moon is from their balance point.
	double hostShare(size_t moon) const;
	// Where a planet's balance point with its moons is, km from the star (a moon's: its planet's).
	Vec balance(size_t body, double days) const;
	// A moon from its planet, km.
	Vec fromHost(size_t moon, double days) const;
	double starRadius() const;        // km
	// The stars, the first or the second (only with a companion). Planets orbit the
	// origin: a close pair's balance point, a wide pair's first star.
	bool hasCompanion() const { return m_spec.star.companion.kind != Pairing::None; }
	Vec starAt(bool second, double days) const; // km
	double secondRadius() const; // km
	// How bright the second is against the first, seen from a body at day `days`.
	double secondShare(size_t body, double days) const;
	double luminosity() const;        // the Sun's = 1
	// The playable body's year: its time round the star (a moon's, its planet's).
	double year() const;
	// The system's day for a world's: as far through the real year as the
	// world is through its own (its calendar's, shorter than the orbit's).
	double dayFor(double worldDays, int32_t worldYear) const;
	// How far a body has turned against the stars, radians, `days` of the system on.
	double turning(size_t body, double days) const;
	// Its time of day (½ noon at longitude 0) as the engine's clock reads:
	// `at` + `turns` × the clock's time of day. A free body's is its own day
	// against 24 hours; a held one's, its orbit's in the system's time.
	struct Clock
	{
		double turns, at;
		double hour(double timeOfDay) const { return at + turns * timeOfDay; }
	};
	Clock clock(size_t body, double worldDays, int32_t worldYear) const;

private:
	// where a body is round what it orbits, km
	Vec local(size_t body, double days) const;
	// the orbit that sets a held body's turning: a planet's partner's, else its own
	size_t heldBy(size_t body) const;

	SystemSpec m_spec;
	std::vector<BodyCard> m_cards;
	size_t m_playable = 0;
};

} // namespace terragen
