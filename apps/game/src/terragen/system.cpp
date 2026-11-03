// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "system.h"
#include "code.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace terragen {

namespace {

constexpr double AU = 149597870.7;        // km
constexpr double EARTH_RADIUS = 6371.0;   // km
constexpr double SUN_RADIUS = 695700.0;   // km
constexpr double GM_SUN = 1.32712440018e11; // km³/s²
constexpr double GM_EARTH = 398600.4418;  // km³/s²
constexpr double DAY = 86400.0;           // s
constexpr double TAU = 6.283185307179586;

double real(fix v) { return v / (double)ONE; }

// a mass given as log₁₀ of Earth's, in hundredths (the page's slider)
fix earths(int hundredths) { return (fix)std::lround(std::pow(10.0, hundredths / 100.0) * ONE); }

SystemBody make(const char *id, const char *name, const char *colour, int32_t parent, double phase, double orbit, double e, int mass,
		double formed, double volatiles, double day, double tilt, bool life = false, bool playable = false)
{
	const auto f = [](double v) { return (fix)std::lround(v * ONE); };
	return { id, name, colour, { parent, f(orbit), f(e), earths(mass), f(formed), f(volatiles), f(day), life }, phase, 0, tilt,
		playable };
}

} // namespace

SystemSpec SystemSpec::home()
{
	SystemSpec s;
	s.star = { 2, ONE, 0, (fix)std::lround(4.5 * ONE), HALF }; // G: the Sun, 4.5 billion years, a middling wind
	s.bodies = {
		make("cinder", "Cinder", "#a76c55", -1, 0.18, 0.42, 0.2, -122, 0.1, 0.05, 1400, 1),
		make("earth", "Earth", "#50a4c5", -1, 0.62, 1, 0.017, 0, 0.35, 0.6, 24, 23, true, true),
		make("vesper", "Vesper", "#b47d5e", -1, 0.88, 1.6, 0.09, -90, 0.45, 0.5, 25, 25),
		make("jovian", "Jovian", "#d7ad83", -1, 0.35, 5.2, 0.05, 250, 0.8, 0.6, 10, 3),
		make("moon", "Moon", "#a8a8a1", 1, 0.1, 60, 0.055, -191, 0.35, 0.05, 655, 7),
		make("aster", "Aster", "#9a9188", 2, 0.4, 6, 0.02, -240, 0.5, 0.05, 30, 0),
		make("pyre", "Pyre", "#ce9464", 3, 0.2, 5.9, 0.0041, -182, 0.55, 0.05, 42, 0),
		make("nacre", "Nacre", "#c9d6da", 3, 0.55, 9.4, 0.009, -210, 0.8, 0.6, 85, 0),
		make("hale", "Hale", "#a9b9b4", 3, 0.75, 15, 0.0013, -160, 0.8, 0.5, 172, 0),
		make("morrow", "Morrow", "#868a91", 3, 0.05, 26, 0.007, -173, 0.85, 0.4, 400, 0),
	};
	s.bodies[4].inclination = 5; // the Moon's orbit, off the ecliptic as ours is
	return s;
}

namespace {

// the seed's dice: each roll from the last, nothing from the machine
class Dice
{
public:
	explicit Dice(uint64_t seed) : m_state(seed) {}
	uint32_t next()
	{
		uint64_t z = (m_state += 0x9E3779B97F4A7C15ull);
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
		return (uint32_t)((z ^ (z >> 31)) >> 32);
	}
	int upTo(int lo, int hi) { return lo + (int)(next() % (uint32_t)(hi - lo + 1)); } // inclusive
	fix between(fix lo, fix hi) { return lo + (fix)(((uint64_t)next() * (uint64_t)(hi - lo)) >> 32); }
	// evenly in the logarithm
	fix span(fix lo, fix hi) { return exp2(between(log2(lo), log2(hi))); }
	bool chance(int percent) { return (int)(next() % 100) < percent; }

private:
	uint64_t m_state;
};

std::string syllabic(Dice &dice)
{
	static const char *const OPEN[] = { "Ar", "Bel", "Cal", "Dor", "El", "Fen", "Gal", "Hal", "Ir", "Kor", "Lum", "Mar", "Nym", "Or", "Pel", "Quil", "Ros", "Sel", "Tar", "Ul", "Vel", "Wen", "Yar", "Zor" };
	static const char *const CLOSE[] = { "a", "is", "on", "ia", "us", "en", "ara", "ith", "ane", "or", "ix", "ene", "yn", "os", "eth", "ia" };
	return std::string(OPEN[dice.next() % 24]) + CLOSE[dice.next() % 16];
}

const char *const SWATCHES[] = { "#a76c55", "#b47d5e", "#d7ad83", "#a8a8a1", "#9a9188", "#ce9464", "#c9d6da", "#a9b9b4", "#868a91", "#ba9678" };

constexpr fix DAY_LEAST = 4 * ONE;

} // namespace

Companion SystemSpec::companionOf(const WorldCode &code)
{
	if (code.companion.kind != Pairing::None || code.style != SystemStyle::Binary)
		return code.companion;
	Dice dice(((uint64_t)code.seed_hash << 8) ^ 0xB1);
	const int primary = (int)(code.star - STARS);
	Companion c;
	c.type = dice.upTo(0, primary);
	const Star &star = STARS[c.type];
	// never heavier than the first, so it outlives it
	c.mass = dice.between(star.least, std::min(star.most, code.star_mass));
	const fix warm = clamp(sqrt(luminosity(code.star_mass) + luminosity(c.mass)), ratio(6, 100), toFix(90));
	// (twice what would touch, Rm^0.8 each)
	const fix touching = 2 * mul(ratio(465, 100000), mul(ONE, exp2(mul(ratio(80, 100), log2(code.star_mass))) + exp2(mul(ratio(80, 100), log2(c.mass)))));
	const fix lo = std::max<fix>(ratio(2, 100), touching), hi = std::min<fix>(ONE, warm / 4);
	if (lo < hi && dice.chance(50)) {
		c.kind = Pairing::Close;
		c.separation = dice.span(lo, hi);
	} else {
		c.kind = Pairing::Wide;
		c.separation = dice.span(toFix(300), toFix(2000)); // (past any planet's reach: 100 AU, a third of 300)
	}
	return c;
}

SystemSpec SystemSpec::generate(const WorldCode &code)
{
	Dice dice(((uint64_t)code.seed_hash << 8) ^ (uint64_t)code.style);
	const auto phase = [&] { return dice.next() / 4294967296.0; };
	SystemSpec s;
	s.star = { (int32_t)(code.star - STARS), code.star_mass, code.metallicity, code.age, code.activity, companionOf(code) };
	const bool harsh = code.style == SystemStyle::Harsh;
	// metal to build from: what's in the star sizes what forms round it
	const Companion other = companionOf(code);
	const fix lit = luminosity(code.star_mass) + (other.kind == Pairing::Close ? luminosity(other.mass) : 0);
	// how near the stars a planet's orbit holds
	const fix nearest = std::max<fix>(ratio(5, 100), other.kind == Pairing::Close ? mul(ratio(31, 10), other.separation) : 0);
	const fix warm = clamp(sqrt(lit), ratio(6, 100), toFix(90)); // AU where Earth's sunlight falls
	const fix frost = clamp(mul(warm, ratio(27, 10)), ratio(10, 100), toFix(95));
	int made = 0;
	const auto body = [&](const std::string &id, const std::string &name, int32_t parent, fix orbit, fix e, fix mass, fix formed, fix volatiles, fix day, double tilt, bool life, bool playable) {
		const char *swatch = SWATCHES[made++ % 10];
		s.bodies.push_back({ id, name, swatch, { parent, orbit, e, mass, clamp(formed, 0, ONE), clamp(volatiles, 0, ONE), std::max(day, DAY_LEAST), life },
			phase(), (double)roundInt(dice.between(-toFix(4), toFix(4))), tilt, playable });
	};
	const auto nextId = [&] { return "b" + std::to_string(made); };
	const auto reach = [&](fix au) { return clamp(au, ratio(5, 100), toFix(100)); };
	const auto whereFormed = [&](fix au) { return mul(ratio(90, 100), std::min<fix>(div(au, mul(frost, toFix(2))), ONE)) + ratio(5, 100); };

	// the planets, in from the star; the home is placed first so it's the anchor
	struct Planet { fix orbit, mass; bool giant; std::string id; };
	std::vector<Planet> planets;
	const fix home_orbit = reach(harsh ? mul(warm, dice.between(ratio(115, 100), ratio(140, 100))) : mul(warm, dice.between(ratio(100, 100), ratio(110, 100)))); // (inside it, seas soon boil away)
	const fix home_mass = harsh ? dice.span(ratio(30, 100), ratio(60, 100)) : dice.span(ratio(80, 100), ratio(130, 100));
	body("earth", "Earth", -1, home_orbit, dice.between(0, ratio(3, 100)), home_mass, ratio(35, 100),
		harsh ? dice.between(ratio(20, 100), ratio(40, 100)) : dice.between(ratio(55, 100), ratio(65, 100)),
		dice.between(toFix(18), toFix(36)), (double)code.tilt / ONE, true, true);
	planets.push_back({ home_orbit, home_mass, false, "earth" });

	// inside: each a step in from the last, while there's room by the star
	fix at = home_orbit;
	for (int n = dice.upTo(0, 3); n > 0; n--) {
		at = mul(at, dice.between(ratio(50, 100), ratio(70, 100)));
		if (at < nearest)
			break;
		const fix mass = dice.span(ratio(5, 100), ONE);
		const std::string id = nextId();
		body(id, syllabic(dice), -1, at, dice.between(0, ratio(20, 100)), mass, whereFormed(at), dice.between(ratio(2, 100), ratio(40, 100)),
			dice.span(toFix(10), toFix(2000)), roundInt(dice.between(0, toFix(30))), false, false);
		planets.push_back({ at, mass, false, id });
	}
	// outside, to the frost line: small and thin
	at = home_orbit;
	for (int n = dice.upTo(0, 2); n > 0; n--) {
		at = mul(at, dice.between(ratio(140, 100), ratio(190, 100)));
		if (at >= frost || at > toFix(100))
			break;
		const fix mass = dice.span(ratio(5, 100), ratio(60, 100));
		const std::string id = nextId();
		body(id, syllabic(dice), -1, at, dice.between(0, ratio(12, 100)), mass, whereFormed(at), dice.between(ratio(20, 100), ratio(60, 100)),
			dice.span(toFix(12), toFix(60)), roundInt(dice.between(0, toFix(40))), false, false);
		planets.push_back({ at, mass, false, id });
	}
	// past it: giants, less of them round a metal-poor star
	at = std::max(at, frost);
	const int giants = dice.upTo(code.metallicity < -HALF ? 0 : 1, code.metallicity < 0 ? 2 : 3);
	for (int n = giants; n > 0; n--) {
		at = mul(at, dice.between(ratio(140, 100), ratio(220, 100)));
		if (at > toFix(100))
			break;
		const fix mass = dice.span(toFix(15), toFix(350));
		const std::string id = nextId();
		body(id, syllabic(dice), -1, at, dice.between(0, ratio(8, 100)), mass, ratio(80, 100), dice.between(ratio(40, 100), ratio(80, 100)),
			dice.between(toFix(9), toFix(20)), roundInt(dice.between(0, toFix(30))), false, false);
		planets.push_back({ at, mass, true, id });
	}

	// the moons, after every planet (a planet is listed before its moons)
	const size_t planet_count = s.bodies.size();
	for (size_t p = 0; p < planet_count; p++) {
		const SystemBody host = s.bodies[p];
		const bool giant = host.causes.mass >= toFix(10);
		const int moons = host.playable ? (code.style == SystemStyle::SunLike ? 1 : dice.upTo(0, 2))
				: giant ? dice.upTo(2, 5) : dice.chance(25) ? 1 : 0;
		fix distance = dice.between(giant ? toFix(4) : toFix(20), giant ? toFix(7) : toFix(60));
		for (int m = 0; m < moons; m++) {
			const std::string id = nextId();
			const fix mass = giant ? dice.span(ratio(1, 100), ratio(4, 100)) : dice.span(ratio(4, 1000), ratio(15, 1000));
			body(id, syllabic(dice), (int32_t)p, std::min<fix>(distance, toFix(190)), dice.between(0, ratio(6, 100)), mass, host.causes.formed,
				dice.between(ratio(3, 100), giant ? ratio(60, 100) : ratio(20, 100)), dice.span(toFix(30), toFix(700)), 0, false, false);
			distance = mul(distance, dice.between(ratio(150, 100), ratio(200, 100)));
		}
	}
	// a second world round a shared point, the same stuff as the first
	if (code.style == SystemStyle::Double) {
		const SystemBody &home = s.bodies[0];
		body(nextId(), syllabic(dice), 0, toFix(10), ratio(1, 100), home.causes.mass, home.causes.formed, home.causes.volatiles, home.causes.day, 15, true, false);
	}
	return s;
}

SystemSpec SystemSpec::read(const std::vector<uint8_t> &b)
{
	size_t at = 0;
	const auto byte = [&]() -> uint8_t {
		if (at >= b.size())
			throw CodeError("This world code's system is damaged.");
		return b[at++];
	};
	const auto word = [&]() { const int lo = byte(); return (int16_t)(uint16_t)(lo | byte() << 8); };
	const auto text = [&]() { std::string t; for (int n = byte(); n > 0; n--) t.push_back((char)byte()); return t; };
	// a value over orders of magnitude: log₁₀ in 8000ths, kept in the model's range however it rounds
	const auto logged = [&](fix lo, fix hi) {
		return clamp(exp2(mul((fix)(((int64_t)word() * ONE) / 8000), ratio(33219, 10000))), lo, hi);
	};
	SystemSpec s;
	const size_t count = byte();
	if (count > MOST_BODIES)
		throw CodeError("This world code's system has too many bodies.");
	for (size_t i = 0; i < count; i++) {
		SystemBody o;
		o.id = text();
		o.name = text();
		static const char HEX[] = "0123456789abcdef";
		o.colour = "#";
		for (int c = 0; c < 3; c++) {
			const uint8_t v = byte();
			o.colour += HEX[v >> 4];
			o.colour += HEX[v & 15];
		}
		const uint8_t parent = byte();
		const uint8_t flags = byte();
		BodyCauses &c = o.causes;
		c.parent = parent == 255 ? -1 : parent;
		c.life = flags & 1;
		const uint8_t spin = flags >> 3;
		if (spin >= (uint8_t)Spin::Count)
			throw CodeError("This world code names a spin this game doesn't have.");
		c.spin = (Spin)spin;
		o.playable = flags & 2;
		c.orbit = c.parent < 0 ? logged(ratio(5, 100), toFix(100)) : logged(ratio(150, 100), toFix(200));
		c.mass = logged(ratio(25, 10000), toFix(400));
		c.day = logged(ONE, toFix(6000));
		c.eccentricity = std::min<fix>(ratio(byte(), 255), ratio(90, 100));
		c.formed = ratio(byte(), 255);
		c.volatiles = ratio(byte(), 255);
		o.phase = byte() / 255.0;
		o.inclination = (int8_t)byte();
		o.tilt = byte() / 2.0;
		if (flags & 4) {
			LandBytes own{};
			for (uint8_t &v : own)
				v = byte();
			o.land = own;
		}
		s.bodies.push_back(std::move(o));
	}
	if (at != b.size())
		throw CodeError("This world code's system is damaged.");
	return s;
}

SystemSpec SystemSpec::forWorld(const WorldCode &code)
{
	SystemSpec s = !code.bodies.empty() ? read(code.bodies) : code.style == SystemStyle::Home ? home() : generate(code);
	s.star = { (int32_t)(code.star - STARS), code.star_mass, code.metallicity, code.age, code.activity, companionOf(code) };
	if (code.bodies.empty()) // (an edited system's bodies have their own)
		for (SystemBody &b : s.bodies)
			if (b.playable)
				b.tilt = real(code.tilt);
	return s;
}

double System::dayFor(double worldDays, int32_t worldYear) const
{
	if (worldYear <= 0)
		throw std::invalid_argument("a world's year has days");
	return worldDays / worldYear * year();
}

double System::year() const
{
	const int32_t parent = m_spec.bodies[m_playable].causes.parent;
	return period(parent < 0 ? m_playable : (size_t)parent);
}

System::System(SystemSpec spec) : m_spec(std::move(spec))
{
	std::vector<BodyCauses> causes;
	size_t playable = 0;
	for (size_t i = 0; i < m_spec.bodies.size(); i++) {
		// held by tides, its axis stands square to the orbit they hold it on
		if (m_spec.bodies[i].causes.spin == Spin::Partner || m_spec.bodies[i].causes.spin == Spin::Star)
			m_spec.bodies[i].tilt = 0;
		causes.push_back(m_spec.bodies[i].causes);
		if (m_spec.bodies[i].playable) {
			m_playable = i;
			playable++;
		}
	}
	if (m_spec.bodies.size() > MOST_BODIES)
		throw std::invalid_argument("a system has at most " + std::to_string(MOST_BODIES) + " bodies");
	if (playable != 1)
		throw std::invalid_argument("a system has one playable body, not " + std::to_string(playable));
	m_cards = bodies(m_spec.star, causes);
}

double System::radius(size_t body) const
{
	return real(m_cards.at(body).radius) * EARTH_RADIUS;
}

double System::luminosity() const
{
	return real(terragen::luminosity(m_spec.star.mass));
}

double System::starRadius() const
{
	return SUN_RADIUS * std::pow(real(m_spec.star.mass), 0.8);
}

System::Vec System::starAt(bool second, double days) const
{
	const Companion &c = m_spec.star.companion;
	if (c.kind == Pairing::None) {
		if (second)
			throw std::logic_error("a system with one star has no second");
		return { 0, 0, 0 };
	}
	const double a = real(c.separation), m1 = real(m_spec.star.mass), m2 = real(c.mass);
	const double turn = TAU * days / (365.25 * std::sqrt(a * a * a / (m1 + m2)));
	const double r = a * AU;
	const Vec apart{ r * std::cos(turn), 0, r * std::sin(turn) };
	const double k = c.kind == Pairing::Wide ? (second ? 1 : 0) : second ? m1 / (m1 + m2) : -m2 / (m1 + m2);
	return { apart.x * k, 0, apart.z * k };
}

double System::secondRadius() const
{
	return SUN_RADIUS * std::pow(real(m_spec.star.companion.mass), 0.8);
}

double System::secondShare(size_t body, double days) const
{
	const Vec p = position(body, days), a = starAt(false, days), b = starAt(true, days);
	const auto apart = [&](const Vec &o) { return (p.x - o.x) * (p.x - o.x) + (p.y - o.y) * (p.y - o.y) + (p.z - o.z) * (p.z - o.z); };
	return real(terragen::luminosity(m_spec.star.companion.mass)) / apart(b) / (luminosity() / apart(a));
}

double System::period(size_t body) const
{
	const BodyCauses &c = m_spec.bodies.at(body).causes;
	if (c.parent < 0) {
		const double a = real(c.orbit) * AU;
		return TAU * std::sqrt(a * a * a / (GM_SUN * real(starsPull(m_spec.star)))) / DAY;
	}
	const double a = real(c.orbit) * radius((size_t)c.parent);
	const double gm = GM_EARTH * (real(m_spec.bodies[(size_t)c.parent].causes.mass) + real(c.mass));
	return TAU * std::sqrt(a * a * a / gm) / DAY;
}

std::optional<size_t> System::partner(size_t body) const
{
	std::vector<BodyCauses> causes;
	for (const SystemBody &b : m_spec.bodies)
		causes.push_back(b.causes);
	const int32_t found = partnerOf(causes, body);
	return found < 0 ? std::nullopt : std::optional<size_t>((size_t)found);
}

double System::hostShare(size_t moon) const
{
	const BodyCauses &c = m_spec.bodies.at(moon).causes;
	if (c.parent < 0)
		throw std::invalid_argument("a planet has no host");
	const double host = real(m_spec.bodies[(size_t)c.parent].causes.mass);
	return host / (host + real(c.mass));
}

size_t System::heldBy(size_t body) const
{
	const BodyCauses &c = m_spec.bodies.at(body).causes;
	if (c.parent >= 0 || c.spin != Spin::Partner)
		return body;
	const std::optional<size_t> other = partner(body);
	if (!other)
		throw std::logic_error("a planet held to no partner"); // (the body model refused this)
	return *other;
}

double System::turning(size_t body, double days) const
{
	const SystemBody &b = m_spec.bodies.at(body);
	const size_t held = heldBy(body);
	const double round = m_spec.bodies[held].phase + days / period(held); // orbits made
	switch (b.causes.spin) {
	case Spin::Partner:
	case Spin::Star: return TAU * round;
	case Spin::Res3_2:
	case Spin::Res2_1:
	case Spin::Res5_2: return TAU * round * real(resonance(b.causes.spin));
	case Spin::Free:
	case Spin::Count: break;
	}
	return TAU * std::fmod(days * 24 / real(b.causes.day), 1.0);
}

System::Clock System::clock(size_t body, double worldDays, int32_t worldYear) const
{
	const SystemBody &b = m_spec.bodies.at(body);
	const double whole = std::floor(worldDays);
	// (`at` holds the whole days, so a clock that isn't the engine's doesn't restart each midnight)
	if (b.causes.spin == Spin::Free) { // 24 hours: the engine's clock itself
		const double turns = 24 / real(b.causes.day);
		return { turns, std::fmod(turns * whole, 1.0) };
	}
	if (b.causes.spin == Spin::Star) // noon at longitude 0, for good
		return { 0, 0.5 };
	if (worldYear <= 0)
		throw std::invalid_argument("a world's year has days");
	// noon where longitude 0 faces the star: how far it has turned against its
	// way round the star (a moon's, its planet's), in the system's compressed time
	const size_t round_star = b.causes.parent < 0 ? body : (size_t)b.causes.parent;
	const double each = b.causes.spin == Spin::Partner ? 1 : real(resonance(b.causes.spin));
	const size_t held = heldBy(body);
	const double turns = (each / period(held) - 1 / period(round_star)) * year() / worldYear;
	return { turns, std::fmod(0.5 + each * m_spec.bodies[held].phase - m_spec.bodies[round_star].phase + turns * whole, 1.0) };
}

System::Vec System::local(size_t body, double days) const
{
	const SystemBody &b = m_spec.bodies.at(body);
	const double e = real(b.causes.eccentricity);
	const double a = b.causes.parent < 0 ? real(b.causes.orbit) * AU : real(b.causes.orbit) * radius((size_t)b.causes.parent);
	// the mean anomaly, then the eccentric (Kepler's equation, by Newton's method)
	double M = TAU * (b.phase + days / period(body));
	M -= TAU * std::floor(M / TAU);
	double E = e < 0.8 ? M : M_PI;
	for (int k = 0; k < 12; k++)
		E -= (E - e * std::sin(E) - M) / (1 - e * std::cos(E));
	const double x = a * (std::cos(E) - e), z = a * std::sqrt(1 - e * e) * std::sin(E);
	// tilted out of its parent's plane, about the line x
	const double i = b.inclination * M_PI / 180;
	return { x, z * std::sin(i), z * std::cos(i) };
}

System::Vec System::balance(size_t body, double days) const
{
	const int32_t parent = m_spec.bodies.at(body).causes.parent;
	return local(parent < 0 ? body : (size_t)parent, days);
}

System::Vec System::fromHost(size_t moon, double days) const
{
	if (m_spec.bodies.at(moon).causes.parent < 0)
		throw std::invalid_argument("a planet has no host");
	return local(moon, days);
}

System::Vec System::position(size_t body, double days) const
{
	const int32_t parent = m_spec.bodies.at(body).causes.parent;
	const size_t host = parent < 0 ? body : (size_t)parent;
	Vec p = local(host, days);
	// the planet swings round its balance point with each moon, opposite it
	for (size_t i = host + 1; i < m_spec.bodies.size(); i++) {
		if (m_spec.bodies[i].causes.parent != (int32_t)host)
			continue;
		const Vec r = local(i, days);
		const double pull = 1 - hostShare(i);
		p = { p.x - r.x * pull, p.y - r.y * pull, p.z - r.z * pull };
		if (i == body)
			p = { p.x + r.x, p.y + r.y, p.z + r.z };
	}
	return p;
}

} // namespace terragen
