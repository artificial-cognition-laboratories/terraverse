// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "code.h"
#include "system.h"
#include "worlds.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <initializer_list>
#include <optional>
#include <vector>

namespace terragen {

const Star STARS[STAR_COUNT] = {
	{ "M", ratio(8, 100), ratio(30, 100), ratio(45, 100) },
	{ "K", ratio(45, 100), ratio(75, 100), ratio(80, 100) },
	{ "G", ratio(80, 100), ONE, ratio(104, 100) },
	{ "F", ratio(104, 100), ratio(120, 100), ratio(140, 100) },
	{ "A", ratio(140, 100), ratio(170, 100), ratio(210, 100) },
	{ "B", ratio(210, 100), toFix(5), toFix(16) },
	{ "O", toFix(16), toFix(30), toFix(60) },
};

fix luminosity(fix mass)
{
	static const fix MOST = toFix(32000); // below the largest fix, for sunlight to be worked from
	// the main sequence's usual fit; mass⁴ through the Sun, so it's exactly 1
	if (mass >= ratio(43, 100) && mass < toFix(2))
		return pow(mass, toFix(4));
	const fix lg = log2(mass);
	const fix lit = mass < ONE
		? exp2(log2(ratio(23, 100)) + mul(ratio(23, 10), lg))
		: exp2(log2(ratio(14, 10)) + mul(ratio(35, 10), lg));
	return std::min(lit, MOST);
}

fix lifetime(fix mass)
{
	return std::max<fix>(ratio(3, 1000), mul(toFix(10), exp2(-mul(ratio(25, 10), log2(mass)))));
}

// Blocks round the equator (an atlas of radius size / 2π: radii 2.6k, 10k,
// 25k, 50k, 100k, 250k, 500k, 1M, 2M, 3.2M and the Earth's 6.371M). Must
// match ui/src/worldgen/config.ts SIZES.
static const int32_t SIZES[] = { 16384, 62832, 157088, 314160, 628320, 1570800, 3141600, 6283184, 12566368,
	20106192, 40030176 };

/*
	The quantized fields, in the order the code stores them, each with its
	range and the byte its default quantizes to (what config.ts DEFAULTS
	encode to; checked by the unit tests against codes the page produced).
*/
struct Field
{
	int32_t min, max;
	uint8_t default_byte;
};

enum FieldIndex { CONTINENTS, LAND, COAST, MOUNTAINS, ROUGHNESS, TEMPERATURE, HUMIDITY, RIVERS, CAVES, ORES, TILT, YEAR, FIELD_COUNT };

static_assert(ORES + 1 == LAND_FIELDS);

static const Field FIELDS[FIELD_COUNT] = {
	{ 1, 8, 219 },   // continents 7
	{ 0, 1, 64 },    // land 0.25
	{ 0, 1, 128 },   // coast 0.5
	{ 0, 1, 128 },   // mountains 0.5
	{ 0, 1, 128 },   // roughness 0.5
	{ -1, 1, 128 },  // temperature 0
	{ -1, 1, 128 },  // humidity 0
	{ 0, 1, 128 },   // rivers 0.5
	{ 0, 1, 128 },   // caves 0.5
	{ 0, 1, 128 },   // ores 0.5
	{ 0, 45, 130 },  // tilt 23
	{ 8, 128, 85 },  // year 48
};
static const uint8_t DEFAULT_SIZE = 0; // 16384: ui/src/worldgen/config.ts DEFAULTS
static const uint8_t DEFAULT_STAR = 2; // G
// The bytes after the star's class, each meaning its usual value exactly
static const uint8_t USUAL_BYTE = 128;   // mass, age; the wind's a half
static const uint8_t SOLAR_METAL = 170;  // [Fe/H] 0

// min + b/255 · (max − min), in fixed point
static fix unquantize(const Field &f, uint8_t b)
{
	return toFix(f.min) + (fix)(((int64_t)b * (f.max - f.min) * ONE) / 255);
}

// A value spanning orders of magnitude, its byte's middle exactly `usual`:
// below it down to `least`, above it up to `most`, evenly in the logarithm.
static fix spanned(fix least, fix usual, fix most, uint8_t b)
{
	if (b == USUAL_BYTE)
		return usual;
	const fix t = b < USUAL_BYTE ? ratio(USUAL_BYTE - b, USUAL_BYTE) : ratio(b - USUAL_BYTE, 255 - USUAL_BYTE);
	return exp2(lerp(log2(usual), log2(b < USUAL_BYTE ? least : most), t));
}

static std::vector<uint8_t> base64url(const std::string &text)
{
	std::vector<uint8_t> out;
	uint32_t acc = 0;
	int bits = 0;
	for (char ch : text) {
		int v;
		if (ch >= 'A' && ch <= 'Z') v = ch - 'A';
		else if (ch >= 'a' && ch <= 'z') v = ch - 'a' + 26;
		else if (ch >= '0' && ch <= '9') v = ch - '0' + 52;
		else if (ch == '-') v = 62;
		else if (ch == '_') v = 63;
		else throw CodeError("This world code has characters a code can't contain.");
		acc = (acc << 6) | (uint32_t)v;
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			out.push_back((uint8_t)(acc >> bits));
		}
	}
	return out;
}

uint32_t hashOf(const std::string &bytes)
{
	uint32_t h = 2166136261u;
	for (unsigned char b : bytes) {
		h ^= b;
		h *= 16777619u;
	}
	return h;
}

// The world is its home body's: what it's like comes from that body's card, so
// everything that decodes a code plays the same world. Its grid is kept by what
// the card decided, so moving a moon doesn't rebuild land the moon doesn't touch.
static void followCard(WorldCode &w)
{
	std::optional<System> system;
	try {
		system.emplace(SystemSpec::forWorld(w));
	} catch (const std::invalid_argument &e) {
		throw CodeError(std::string("This world code's system can't exist: ") + e.what());
	}
	const size_t home = system->playable();
	const BodyCard &card = system->cards()[home];
	if (card.giant)
		throw CodeError("This world code starts on a giant, which has no surface to stand on.");
	const SystemBody &body = system->spec().bodies[home];
	w.processes = processesOf(card, body.causes.life);
	if (!w.bodies.empty())
		w.tilt = (fix)std::lround(body.tilt * ONE);
	w.keepGrid();
}

void WorldCode::keepGrid()
{
	const Processes &p = processes;
	grid_key = seed;
	for (const int64_t v : std::initializer_list<int64_t>{ size, continents, land, coast, mountains, roughness, temperature, humidity, rivers, caves, ores, tilt, year,
			p.day, p.facing_star, p.carrying, (int64_t)p.regime, p.water, p.life, (int64_t)p.bare, p.craters, p.smallest_crater, p.gravity,
			p.flooding, p.young, p.stiff, p.flexing, p.plumes, p.ancient_water, p.eruptions, p.warming })
		grid_key += "," + std::to_string(v);
}

LandBytes defaultLand()
{
	LandBytes out{};
	for (size_t i = 0; i < LAND_FIELDS; i++)
		out[i] = FIELDS[i].default_byte;
	return out;
}

void WorldCode::setLand(const LandBytes &b)
{
	continents = roundInt(unquantize(FIELDS[CONTINENTS], b[CONTINENTS]));
	land = unquantize(FIELDS[LAND], b[LAND]);
	coast = unquantize(FIELDS[COAST], b[COAST]);
	mountains = unquantize(FIELDS[MOUNTAINS], b[MOUNTAINS]);
	roughness = unquantize(FIELDS[ROUGHNESS], b[ROUGHNESS]);
	temperature = unquantize(FIELDS[TEMPERATURE], b[TEMPERATURE]);
	humidity = unquantize(FIELDS[HUMIDITY], b[HUMIDITY]);
	rivers = unquantize(FIELDS[RIVERS], b[RIVERS]);
	caves = unquantize(FIELDS[CAVES], b[CAVES]);
	ores = unquantize(FIELDS[ORES], b[ORES]);
}

WorldCode WorldCode::decode(const std::string &code)
{
	size_t first = code.find('.');
	if (first == std::string::npos)
		throw CodeError("This isn't a world code.");
	std::string version = code.substr(0, first);
	if (version == "TF18" || version == "TF17" || version == "TF16" || version == "TF15" || version == "TF14" || version == "TF13" || version == "TF12" || version == "TF11" || version == "TF10" || version == "TF9" || version == "TF1" || version == "TF2" || version == "TF3" || version == "TF4" || version == "TF5" || version == "TF6" || version == "TF7" || version == "TF8")
		throw CodeError("This world code is from an older build of Terraverse (" + version + "); its worlds can't be generated any more.");
	if (version != "TF19")
		throw CodeError("This isn't a world code this game reads (TF19).");
	size_t second = code.find('.', first + 1);
	std::string seed_part = code.substr(first + 1, second == std::string::npos ? std::string::npos : second - first - 1);
	if (seed_part.empty())
		throw CodeError("This world code has no seed.");

	std::vector<uint8_t> seed = base64url(seed_part);
	uint8_t size = DEFAULT_SIZE, star = DEFAULT_STAR;
	uint8_t style = 0;
	uint8_t mass = USUAL_BYTE, metal = SOLAR_METAL, age = USUAL_BYTE, wind = USUAL_BYTE;
	std::vector<uint8_t> bodies, pair;
	uint8_t bytes[FIELD_COUNT];
	for (int i = 0; i < FIELD_COUNT; i++)
		bytes[i] = FIELDS[i].default_byte;

	if (second != std::string::npos) {
		const size_t third = code.find('.', second + 1);
		std::vector<uint8_t> params = base64url(code.substr(second + 1, third == std::string::npos ? std::string::npos : third - second - 1));
		if (third != std::string::npos)
			bodies = base64url(code.substr(third + 1));
		if (params.size() != FIELD_COUNT + 7 && params.size() != FIELD_COUNT + 11)
			throw CodeError("This world code is damaged, or from an older build.");
		size = params[0];
		star = params[1];
		mass = params[2];
		metal = params[3];
		age = params[4];
		wind = params[5];
		style = params[6];
		for (int i = 0; i < FIELD_COUNT; i++)
			bytes[i] = params[i + 7];
		if (params.size() == FIELD_COUNT + 11)
			pair.assign(params.begin() + FIELD_COUNT + 7, params.end());
	}
	if (size >= sizeof(SIZES) / sizeof(SIZES[0]) || star >= sizeof(STARS) / sizeof(STARS[0]))
		throw CodeError("This world code names a world size or star this game doesn't have.");

	if (style >= (uint8_t)SystemStyle::Count)
		throw CodeError("This world code names a kind of system this game doesn't have.");

	WorldCode w;
	w.text = code;
	w.style = (SystemStyle)style;
	w.bodies = std::move(bodies);
	w.seed.assign(seed.begin(), seed.end());
	w.seed_hash = hashOf(w.seed);
	w.size = SIZES[size];
	w.star = &STARS[star];
	w.star_mass = spanned(w.star->least, w.star->mass, w.star->most, mass);
	w.metallicity = ((fix)metal - SOLAR_METAL) * ONE / SOLAR_METAL;
	// never past the star's life, which a byte's rounding could put it: the same code, the same age
	w.age = std::min(spanned(ratio(2, 1000), (fix)std::lround(4.5 * ONE), toFix(13), age), lifetime(w.star_mass));
	w.activity = (fix)wind * ONE / 256;
	if (!pair.empty()) {
		// kind, class, mass, and the separation evenly in the logarithm over the kind's reach
		if (pair[0] == 0 || pair[0] >= (uint8_t)Pairing::Count || pair[1] >= STAR_COUNT)
			throw CodeError("This world code names a companion star this game doesn't have.");
		Companion &c = w.companion;
		c.kind = (Pairing)pair[0];
		c.type = pair[1];
		c.mass = spanned(STARS[c.type].least, STARS[c.type].mass, STARS[c.type].most, pair[2]);
		const fix least = c.kind == Pairing::Close ? ratio(2, 100) : toFix(30), most = c.kind == Pairing::Close ? ONE : toFix(2000);
		c.separation = clamp(exp2(lerp(log2(least), log2(most), ratio(pair[3], 255))), least, most);
		w.age = std::min(w.age, lifetime(c.mass));
	}
	LandBytes own{};
	std::copy_n(bytes, LAND_FIELDS, own.begin());
	w.setLand(own);
	w.tilt = toFix(roundInt(unquantize(FIELDS[TILT], bytes[TILT])));
	w.year = roundInt(unquantize(FIELDS[YEAR], bytes[YEAR]));
	followCard(w);
	return w;
}

} // namespace terragen
