// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "code.h"
#include <vector>

namespace terragen {

const Star STARS[5] = {
	{ "red", -ratio(55, 100), ratio(30, 100) },
	{ "orange", -ratio(20, 100), ratio(75, 100) },
	{ "yellow", 0, ONE },
	{ "white", ratio(25, 100), ratio(150, 100) },
	{ "blue", ratio(50, 100), ratio(250, 100) },
};

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

static const Field FIELDS[FIELD_COUNT] = {
	{ 1, 8, 109 },   // continents 4
	{ 0, 1, 102 },   // land 0.4
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
static const uint8_t DEFAULT_STAR = 2; // yellow

// min + b/255 · (max − min), in fixed point
static fix unquantize(const Field &f, uint8_t b)
{
	return toFix(f.min) + (fix)(((int64_t)b * (f.max - f.min) * ONE) / 255);
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

static uint32_t fnv1a(const std::string &bytes)
{
	uint32_t h = 2166136261u;
	for (unsigned char b : bytes) {
		h ^= b;
		h *= 16777619u;
	}
	return h;
}

WorldCode WorldCode::decode(const std::string &code)
{
	size_t first = code.find('.');
	if (first == std::string::npos)
		throw CodeError("This isn't a world code.");
	std::string version = code.substr(0, first);
	if (version == "TF1" || version == "TF2" || version == "TF3" || version == "TF4" || version == "TF5" || version == "TF6" || version == "TF7" || version == "TF8")
		throw CodeError("This world code is from an older build of Terra Firma (" + version + "); its worlds can't be generated any more.");
	if (version != "TF9")
		throw CodeError("This isn't a world code this game reads (TF9).");
	size_t second = code.find('.', first + 1);
	std::string seed_part = code.substr(first + 1, second == std::string::npos ? std::string::npos : second - first - 1);
	if (seed_part.empty())
		throw CodeError("This world code has no seed.");

	std::vector<uint8_t> seed = base64url(seed_part);
	uint8_t size = DEFAULT_SIZE, star = DEFAULT_STAR;
	uint8_t bytes[FIELD_COUNT];
	for (int i = 0; i < FIELD_COUNT; i++)
		bytes[i] = FIELDS[i].default_byte;

	if (second != std::string::npos) {
		std::vector<uint8_t> params = base64url(code.substr(second + 1));
		if (params.size() != FIELD_COUNT + 2)
			throw CodeError("This world code is damaged, or from an older build.");
		size = params[0];
		star = params[1];
		for (int i = 0; i < FIELD_COUNT; i++)
			bytes[i] = params[i + 2];
	}
	if (size >= sizeof(SIZES) / sizeof(SIZES[0]) || star >= sizeof(STARS) / sizeof(STARS[0]))
		throw CodeError("This world code names a world size or star this game doesn't have.");

	WorldCode w;
	w.text = code;
	w.seed.assign(seed.begin(), seed.end());
	w.seed_hash = fnv1a(w.seed);
	w.size = SIZES[size];
	w.star = &STARS[star];
	w.continents = roundInt(unquantize(FIELDS[CONTINENTS], bytes[CONTINENTS]));
	w.land = unquantize(FIELDS[LAND], bytes[LAND]);
	w.coast = unquantize(FIELDS[COAST], bytes[COAST]);
	w.mountains = unquantize(FIELDS[MOUNTAINS], bytes[MOUNTAINS]);
	w.roughness = unquantize(FIELDS[ROUGHNESS], bytes[ROUGHNESS]);
	w.temperature = unquantize(FIELDS[TEMPERATURE], bytes[TEMPERATURE]);
	w.humidity = unquantize(FIELDS[HUMIDITY], bytes[HUMIDITY]);
	w.rivers = unquantize(FIELDS[RIVERS], bytes[RIVERS]);
	w.caves = unquantize(FIELDS[CAVES], bytes[CAVES]);
	w.ores = unquantize(FIELDS[ORES], bytes[ORES]);
	w.tilt = toFix(roundInt(unquantize(FIELDS[TILT], bytes[TILT])));
	w.year = roundInt(unquantize(FIELDS[YEAR], bytes[YEAR]));
	return w;
}

} // namespace terragen
