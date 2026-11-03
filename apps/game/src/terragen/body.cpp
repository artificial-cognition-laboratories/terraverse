// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "body.h"
#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>

namespace terragen {

const char *const KIND_NAMES[(int)Kind::Count] = {
	"temperate", "cold desert", "hothouse", "volcanic", "ice shell", "hydrocarbon", "airless", "giant",
};

const char *const GAS_NAMES[(int)Gas::Count] = { "CO2", "N2", "O2", "CH4", "H2", "He" };

const char *const EVENT_TEXT[(int)Event::Count] = {
	"a magnetic field switches on",
	"the core freezes; the magnetic field fades",
	"oceans condense",
	"the oceans boil away",
	"the oceans freeze",
	"brief seas come and go",
	"plate tectonics begins",
	"the plates lock still",
	"the volcanoes fall silent",
	"the air is lost to space",
	"a runaway greenhouse",
	"life appears in the oceans",
	"oxygen builds up in the air",
};

static fix F(int32_t hundredths) { return ratio(hundredths, 100); }

static fix minx(fix a, fix b) { return a < b ? a : b; }

// a^b that is 0 for a ≤ 0
static fix power(fix a, fix b)
{
	return a <= 0 ? 0 : pow(a, b);
}

/*
	A history is told as changes that last: a state must hold for HOLD before
	it becomes part of the story (dated from when it began). One that keeps
	flipping back and forth is told once, if it has a way to say so.
*/
class Story
{
public:
	struct Thread
	{
		std::optional<Event> on, off; // what each change is called; none isn't news
		std::optional<Event> flickers;
		bool known = false, state = false;
		bool waiting = false, next = false;
		fix since = 0;
		int flips = 0;
		bool told_flicker = false;
	};

	explicit Story(std::vector<Moment> &out) : m_out(out) {}

	// The end of the history: a change still waiting out HOLD did happen
	void finish(Thread &th)
	{
		if (!th.waiting)
			return;
		const std::optional<Event> said = th.next ? th.on : th.off;
		if (said && th.known)
			m_out.push_back({ th.since, *said });
		th.waiting = false;
	}

	void mark(Thread &th, fix t, bool value)
	{
		if (th.known && th.state == value) {
			th.waiting = false;
			return;
		}
		if (!th.waiting || th.next != value) {
			th.waiting = true;
			th.next = value;
			th.since = t;
			if (++th.flips >= 6 && th.flickers && !th.told_flicker) {
				th.told_flicker = true;
				m_out.push_back({ t, *th.flickers });
			}
			return;
		}
		if (t - th.since < HOLD)
			return;
		th.known = true;
		th.state = value;
		th.waiting = false;
		// a starting state that's "off" isn't news; only an ending is
		std::optional<Event> said = value ? th.on : th.off;
		if (said && (value || th.since > F(20)))
			m_out.push_back({ th.since, *said });
	}

private:
	static constexpr fix HOLD = 13107; // 0.2 Gyr
	std::vector<Moment> &m_out;
};

fix starsPull(const StarCauses &star)
{
	return star.companion.kind == Pairing::Close ? star.mass + star.companion.mass : star.mass;
}

// A star's radius in AU (R☉ m^0.8), as System::starRadius has it
static fix starRadiusAU(fix mass)
{
	return mul(ratio(465, 100000), power(mass, F(80)));
}

static void checkCompanion(const StarCauses &star)
{
	const Companion &c = star.companion;
	if (c.kind == Pairing::None)
		return;
	if (c.kind >= Pairing::Count || c.type < 0 || c.type >= STAR_COUNT)
		throw std::invalid_argument("no such companion star");
	if (c.mass < STARS[c.type].least || c.mass > STARS[c.type].most)
		throw std::invalid_argument("a companion's mass must be in its class's range");
	if (star.age > lifetime(c.mass))
		throw std::invalid_argument("a system can't be older than its companion lives");
	if (c.kind == Pairing::Close ? c.separation < F(2) || c.separation > ONE : c.separation < toFix(30) || c.separation > toFix(2000))
		throw std::invalid_argument("a close pair is 0.02..1 AU apart, a wide one 30..2000 AU");
	if (c.separation <= starRadiusAU(star.mass) + starRadiusAU(c.mass))
		throw std::invalid_argument("the two stars would touch: move them apart, or make them smaller");
}

static void check(const StarCauses &star, const BodyCauses &c, size_t i, const std::vector<BodyCauses> &all)
{
	auto fail = [&](const std::string &why) {
		throw std::invalid_argument("body " + std::to_string(i) + ": " + why);
	};
	if (c.parent >= (int32_t)i)
		fail("its parent must be listed before it");
	if (c.parent >= 0 && all[c.parent].parent >= 0)
		fail("a moon can't have moons");
	if (c.mass < F(1) / 4 || c.mass > toFix(400))
		fail("mass must be 0.0025..400 Earth masses");
	if (c.parent < 0 ? c.orbit < F(5) || c.orbit > toFix(100) : c.orbit < F(150) || c.orbit > toFix(200))
		fail("orbit must be 0.05..100 AU around the star, 1.5..200 radii around a planet");
	if (c.eccentricity < 0 || c.eccentricity > F(90))
		fail("eccentricity must be 0..0.9");
	if (c.formed < 0 || c.formed > ONE || c.volatiles < 0 || c.volatiles > ONE)
		fail("formed and volatiles are 0..1");
	if (c.day < ONE)
		fail("a day is at least an hour");
	if (c.spin >= Spin::Count)
		fail("no such spin");
	if (c.parent >= 0 && c.spin == Spin::Star)
		fail("a moon can't keep one face to its star");
	if (c.parent < 0 && c.spin == Spin::Partner && partnerOf(all, i) < 0)
		fail("a planet has no partner to keep one face to");
	// Holman & Wiegert's limits, near enough: inside these the second star flings a planet out
	if (c.parent < 0 && star.companion.kind == Pairing::Close && c.orbit < 3 * star.companion.separation)
		fail("too close to the two stars for a stable orbit: at least three times their separation");
	if (c.parent < 0 && star.companion.kind == Pairing::Wide && c.orbit > star.companion.separation / 3)
		fail("too far out for a stable orbit: the other star pulls it away past a third of their separation");
}

int32_t partnerOf(const std::vector<BodyCauses> &all, size_t body)
{
	const auto outside = [](const BodyCauses &moon, const BodyCauses &host) {
		// (wide: the product is past what a fix holds)
		return ((int64_t)moon.orbit * moon.mass) / ((int64_t)moon.mass + host.mass) > ONE;
	};
	const BodyCauses &self = all.at(body);
	if (self.parent >= 0)
		return outside(self, all.at((size_t)self.parent)) ? self.parent : -1;
	for (size_t i = 0; i < all.size(); i++)
		if (all[i].parent == (int32_t)body && outside(all[i], self))
			return (int32_t)i;
	return -1;
}

static fix cappedHours(fix lg)
{
	return lg >= toFix(15) ? toFix(32000) : std::min(exp2(lg), toFix(32000));
}

// Hours round the star, from Kepler (years = √(a³ / M)), in logs so far orbits fit.
static fix yearHours(const StarCauses &star, const BodyCauses &c)
{
	return cappedHours(log2(toFix(8766)) + (3 * log2(c.orbit) - log2(starsPull(star))) / 2);
}

// Hours a moon and its planet take round each other, both masses pulling: 1.406 h
// at an Earth's radius of an Earth's mass.
static fix pairHours(const BodyCauses &moon, const BodyCauses &host, fix host_radius)
{
	return cappedHours(log2(ratio(1406, 1000)) + (3 * (log2(moon.orbit) + log2(host_radius)) - log2(moon.mass + host.mass)) / 2);
}

// What flexes a body: the other's mass, its own size, the orbit in the planet's radii.
static fix tides(fix other_mass, fix radius, const BodyCauses &moon)
{
	const fix l = 2 * log2(other_mass) + 5 * log2(radius) + log2(moon.eccentricity + ratio(1, 1000))
			- 6 * log2(moon.orbit) + ratio(197, 10);
	return minx(exp2(l), toFix(1000)); // beyond this a moon is molten anyway
}

// Whether the pair can hold together: not touching, nor inside the distance where
// one's tides pull the other apart (the rigid Roche limit, 1.26 R (ρ / ρ')^⅓ = 1.26 R' (M / M')^⅓).
static void checkPair(const BodyCauses &moon, fix moon_radius, const BodyCauses &host, fix host_radius, size_t i)
{
	const auto fail = [&](const std::string &why) {
		throw std::invalid_argument("body " + std::to_string(i) + ": " + why);
	};
	const fix apart = moon.orbit; // in the planet's radii
	const fix size = div(moon_radius, host_radius);
	if (apart <= ONE + size)
		fail("it's touching its planet: move it further out, or make it smaller");
	const fix ratio_cube = exp2((log2(host.mass) - log2(moon.mass)) / 3);
	if (apart < mul(ratio(126, 100), mul(ratio_cube, size)))
		fail("its planet's tides would pull it apart: move it further out");
	if (apart < mul(ratio(126, 100), exp2((log2(moon.mass) - log2(host.mass)) / 3)))
		fail("its tides would pull its planet apart: move it further out, or make it lighter");
}

const char *const SPIN_NAMES[(int)Spin::Count] = { "free", "partner", "star", "3:2", "2:1", "5:2" };

// The turns it makes an orbit, under a resonance; none otherwise.
fix resonance(Spin spin)
{
	switch (spin) {
	case Spin::Res3_2: return ratio(3, 2);
	case Spin::Res2_1: return toFix(2);
	case Spin::Res5_2: return ratio(5, 2);
	default: return 0;
	}
}

std::vector<BodyCard> bodies(const StarCauses &star, const std::vector<BodyCauses> &causes)
{
	if (star.type < 0 || star.type >= STAR_COUNT)
		throw std::invalid_argument("no star type " + std::to_string(star.type));
	if (star.mass < STARS[star.type].least || star.mass > STARS[star.type].most)
		throw std::invalid_argument("a star's mass must be in its class's range");
	if (star.metallicity < -ONE || star.metallicity > HALF)
		throw std::invalid_argument("a star's metallicity is −1..0.5");
	if (star.age < ratio(2, 1000) || star.age > toFix(13))
		throw std::invalid_argument("a system's age is 0.002..13 billion years");
	if (star.age > lifetime(star.mass))
		throw std::invalid_argument("a system can't be older than its star lives");

	checkCompanion(star);
	// a close pair shines as one; a wide companion's light is the same at every planet, what its distance leaves
	const Companion &other = star.companion;
	const fix lit = luminosity(star.mass) + (other.kind == Pairing::Close ? luminosity(other.mass) : 0);
	const fix lg_far = other.kind == Pairing::Wide ? log2(luminosity(other.mass)) - 2 * log2(other.separation) : -toFix(32);
	const fix far_light = lg_far < -toFix(16) ? 0 : exp2(lg_far);
	// metal to build from: as the star's, gently, for what a disc keeps of it
	const fix metals = exp2(mul(mul(star.metallicity, ratio(33219, 10000)), ratio(25, 100)));
	std::vector<BodyCard> cards;
	cards.reserve(causes.size());

	for (size_t i = 0; i < causes.size(); i++) {
		const BodyCauses &c = causes[i];
		check(star, c, i, causes);
		const BodyCauses *parent = c.parent >= 0 ? &causes[c.parent] : nullptr;
		const BodyCard *parent_card = c.parent >= 0 ? &cards[c.parent] : nullptr;
		BodyCard b{};

		// what it's made of, and so its size
		b.giant = (c.mass >= toFix(10) && c.formed >= HALF) || c.mass >= toFix(50);
		if (b.giant) {
			b.metal = F(5);
			b.ice = F(10);
			b.rock = F(15);
			b.radius = F(350) + mul(toFix(8), ONE - exp(-c.mass / 90));
		} else {
			b.ice = clamp(mul(c.formed - HALF, F(160)), 0, F(70));
			b.metal = mul(ONE - b.ice, minx(mul(F(40) - mul(F(20), c.formed), metals), F(60)));
			b.rock = ONE - b.ice - b.metal;
			b.radius = mul(power(c.mass, F(27)), ONE + mul(F(45), b.ice) - mul(F(35), b.metal - F(32)));
		}
		b.gravity = div(c.mass, mul(b.radius, b.radius));
		b.escape = mul(ratio(112, 10), sqrt(div(c.mass, b.radius)));

		// where it is: starlight, and tides from a parent
		fix distance = parent ? parent->orbit : c.orbit; // AU from the star
		// wide: a bright star close in is more than a fix holds, and all the same by then
		b.sunlight = (fix)std::min<int64_t>(((int64_t)lit << FRAC) / mul(distance, distance) + far_light, toFix(32000));
		b.equilibrium = mul(mul(toFix(278), power(b.sunlight, F(25))), ratio(915, 1000));
		if (parent) {
			checkPair(c, b.radius, *parent, parent_card->radius, i);
			b.tidal = tides(parent->mass, b.radius, c);
		}
		// the other half of a double flexes its planet as the planet flexes it
		const int32_t partner = c.parent < 0 ? partnerOf(causes, i) : -1;
		if (partner >= 0 && !b.giant)
			b.tidal = tides(causes[(size_t)partner].mass, b.radius, causes[(size_t)partner]);
		b.spin = c.spin;
		b.locked = c.spin == Spin::Partner || c.spin == Spin::Star;
		// a planet holds to its partner's month, a moon to its planet's, the rest to their year
		const fix round_hours = parent ? pairHours(c, *parent, parent_card->radius)
				: c.spin == Spin::Partner ? pairHours(causes[(size_t)partner], c, b.radius)
				: yearHours(star, c);
		b.day = c.spin == Spin::Free ? c.day
				: b.locked ? round_hours
				: std::max<fix>(div(round_hours, resonance(c.spin)), ONE);
		const fix spin = sqrt(minx(ONE, div(toFix(24), b.day)));
		const fix parent_field = parent_card && parent_card->giant ? parent_card->field : 0;

		// the air and water it formed with
		const bool inner = c.formed < F(70); // inside the frost line: CO₂-rich; beyond: nitrogen and methane
		fix co2 = 0, n2 = 0, o2 = 0, ch4 = 0, water = 0;
		{
			fix first = mul(c.volatiles, toFix(2) + 30 * b.ice);
			if (inner) {
				co2 = 7 * first;
				n2 = mul(F(16), first);
			} else {
				n2 = mul(F(75), first);
				ch4 = mul(F(25), first);
			}
			water = mul(F(150), first);
		}

		auto interior = [&](fix t) {
			fix past = ratio(45, 10) - t;
			return mul(power(c.mass, F(60)),
					mul(F(80), exp2(div(past, F(250)))) + mul(F(20), exp2(div(past, F(120)))));
		};
		auto climate = [&](fix air, fix co2_now, fix &pressure, fix &temperature) {
			pressure = mul(air, b.gravity);
			fix greenhouse = 0;
			if (!b.giant && pressure > 0)
				greenhouse = mul(mul(toFix(33), power(pressure, F(35))), ONE + 2 * div(co2_now, air));
			temperature = b.equilibrium + greenhouse;
		};

		Story story(b.history);
		Story::Thread field_th{ Event::FieldOn, Event::FieldFades, std::nullopt };
		Story::Thread ocean_th{ Event::OceansForm, Event::OceansFreeze, Event::SeasComeAndGo };
		Story::Thread plates_th{ Event::PlatesBegin, Event::PlatesStop, std::nullopt };
		Story::Thread volcano_th{ std::nullopt, Event::VolcanoesSilent, std::nullopt };
		Story::Thread air_th{ std::nullopt, Event::AirLost, std::nullopt };
		bool life_told = false, oxygen_told = false, runaway_told = false;

		fix dt = ratio(5, 100);
		fix t = 0, pressure = 0, temperature = b.equilibrium, heat = 0;
		bool ocean = false, plates = false;
		while (t < star.age) {
			dt = minx(dt, star.age - t); // a young system's whole life can be less than a step
			t += dt;
			heat = interior(t) + b.tidal;

			// volcanoes add air; a warm interior is needed
			fix out = mul(mul(mul(c.volatiles, std::max<fix>(0, minx(heat, toFix(6)) - F(25))), dt), F(90));
			if (inner) {
				co2 += 7 * out;
				n2 += mul(F(16), out);
			} else {
				n2 += 6 * out;
				ch4 += 2 * out;
			}
			water += mul(F(150), out);
			fix air = co2 + n2 + o2 + ch4;
			climate(air, co2, pressure, temperature);

			// the core: cools by its own clock, small ones fast; spinning makes a field
			fix core_heat = mul(12 * c.mass, exp2(-div(t, F(120))));
			bool core = !b.giant && b.metal > F(10) && core_heat > F(35);
			b.field = b.giant ? minx(toFix(20), c.mass / 30)
					: core ? mul(mul(div(b.metal, F(32)), spin), minx(ONE, core_heat)) : 0;

			fix boiling = toFix(373) + 30 * log2(pressure + ONE);
			ocean = b.sunlight < F(130) && temperature > toFix(273) && temperature < boiling &&
					pressure > ratio(6, 1000) && water > F(5);

			// losses: thermal escape (light, hot worlds), the stellar wind where no
			// field shields, and a giant's magnetosphere for its close moons
			// Jeans: half the escape parameter, Earth ≈ 80 (held for good)
			fix hold = minx(mul(toFix(80), power(div(b.escape, ratio(112, 10)), toFix(2))), toFix(100));
			hold = minx(mul(hold, div(toFix(300), std::max<fix>(temperature, toFix(30)))), toFix(100));
			fix thermal = 50 * exp(-hold);
			fix young = ONE + 4 * exp2(-div(t, F(70)));
			// closer than 0.1 AU it strips everything regardless
			fix near = std::max<fix>(mul(distance, distance), F(1));
			fix wind = minx(div(mul(mul(mul(20 * star.activity, young), std::max<fix>(0, ONE - b.field)),
					exp(-power(div(b.escape, toFix(5)), toFix(2)))), near), toFix(1000));
			fix sputter = parent ? mul(3 * parent_field, power(div(toFix(5), c.orbit), toFix(3))) : 0;
			fix loss = minx(F(90), mul(thermal + wind + sputter, dt));
			co2 = mul(co2, ONE - loss);
			n2 = mul(n2, ONE - loss);
			o2 = mul(o2, ONE - loss);
			ch4 = mul(ch4, ONE - loss);
			bool steam = temperature > toFix(330) && !ocean && water > F(5);
			if (steam) {
				water = mul(water, ONE - minx(F(90), mul(F(60), dt) + loss)); // its hydrogen escapes
				// a runaway boils an ocean's worth of water under air that traps the
				// heat (Venus); a world that formed dry (Mercury) just dries further
				if (!runaway_told && b.sunlight >= F(130) && pressure > F(10) && water > HALF) {
					runaway_told = true;
					b.history.push_back({ t, Event::Runaway });
				}
			}
			water = mul(water, ONE - loss);

			plates = !b.giant && ocean && heat > F(40) && heat < toFix(5) && c.mass >= F(30);
			if (ocean) // carbon into rock where plates recycle it, some into the sea otherwise
				co2 = mul(co2, plates ? F(2) : F(70));
			if (c.life && ocean && t > F(80)) {
				if (!life_told) {
					life_told = true;
					b.history.push_back({ t, Event::LifeAppears });
				}
				if (t > F(240))
					o2 = minx(mul(n2, F(27)), o2 + mul(F(10), dt));
			}
			if (o2 > F(5) && !oxygen_told) {
				oxygen_told = true;
				b.history.push_back({ t, Event::OxygenRises });
			}

			if (!b.giant) {
				story.mark(field_th, t, b.field > F(10));
				story.mark(ocean_th, t, ocean);
				story.mark(plates_th, t, plates);
				story.mark(volcano_th, t, heat > F(30));
				story.mark(air_th, t, pressure > ratio(3, 1000));
			}
			climate(co2 + n2 + o2 + ch4, co2, pressure, temperature);
		}
		story.finish(volcano_th);
		std::stable_sort(b.history.begin(), b.history.end(),
				[](const Moment &a, const Moment &z) { return a.age < z.age; });

		b.heat = heat;
		b.ocean = ocean;
		b.plates = plates;
		b.volcanism = heat > F(30);
		b.temperature = temperature;
		fix air = co2 + n2 + o2 + ch4;
		if (b.giant) {
			b.pressure = -ONE;
			b.gases[(int)Gas::H2] = F(86);
			b.gases[(int)Gas::He] = F(14);
		} else {
			b.pressure = pressure;
			if (pressure >= ratio(1, 1000) && air > 0) {
				b.gases[(int)Gas::CO2] = div(co2, air);
				b.gases[(int)Gas::N2] = div(n2, air);
				b.gases[(int)Gas::O2] = div(o2, air);
				b.gases[(int)Gas::CH4] = div(ch4, air);
			}
		}

		// the inside: a core sized by its metal (iron is about twice as dense
		// as the whole), a mantle, a thin crust, then water or ice
		if (b.giant) {
			b.layers = { { "core", F(12), false }, { "metallic hydrogen", F(70), true }, { "envelope", ONE, true } };
		} else {
			fix core = power(mul(b.metal, HALF), ratio(1, 3));
			bool liquid_core = b.field > F(10) || heat > toFix(3);
			// ice is lighter than rock: its share of the volume is half again its share of the mass
			fix ice_volume = minx(mul(b.ice, F(150)), F(80));
			fix ice = b.ice > F(5) ? ONE - power(ONE - ice_volume, ratio(1, 3)) : 0;
			b.layers.push_back({ "core", std::max<fix>(core, F(5)), liquid_core });
			b.layers.push_back({ "mantle", ONE - ice - F(2), false });
			// a surface ocean sits on the crust: a hundredth of the radius, so it shows
			b.layers.push_back({ "crust", ONE - ice - (ice == 0 && b.ocean ? F(1) : 0), false });
			if (ice > 0) {
				// a tidally warmed ice world hides an ocean under its shell
				if (b.tidal > F(30))
					b.layers.push_back({ "ocean", ONE - ice / 3, true });
				b.layers.push_back({ "ice", ONE, false });
			} else if (b.ocean) {
				b.layers.push_back({ "ocean", ONE, true });
			}
		}

		if (b.giant)
			b.kind = Kind::Giant;
		else if (heat > toFix(8))
			b.kind = Kind::Volcanic;
		else if (b.ice > F(30) && b.tidal > F(30) && temperature < toFix(200))
			b.kind = Kind::IceShell;
		else if (pressure < ratio(1, 1000))
			b.kind = Kind::Airless;
		else if (temperature > toFix(400))
			b.kind = Kind::Hothouse;
		else if (ocean)
			b.kind = Kind::Temperate;
		else if (temperature < toFix(140) && pressure > F(30) && ch4 > 0)
			b.kind = Kind::Hydrocarbon;
		else
			b.kind = Kind::ColdDesert;
		cards.push_back(std::move(b));
	}
	return cards;
}

} // namespace terragen
