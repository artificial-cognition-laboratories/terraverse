// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "planetarium/paint.h"
#include "exceptions.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace planetarium {

using namespace terragen;

namespace {

using Rgb = std::array<float, 3>;

// fields.ts's colours, one for one
const Rgb BIOME_COLOURS[(int)Biome::Count] = {
	{ 18, 52, 96 }, { 30, 84, 140 }, { 200, 220, 235 }, { 214, 200, 140 }, { 222, 190, 118 }, { 176, 170, 84 },
	{ 104, 160, 72 }, { 52, 116, 56 }, { 28, 120, 60 }, { 60, 100, 80 }, { 150, 160, 140 }, { 120, 116, 110 },
	{ 236, 240, 244 }, { 96, 140, 40 }, { 160, 150, 96 }, { 168, 166, 160 }, { 84, 84, 88 },
};
const std::vector<Rgb> HEAT_RAMP{ { 40, 60, 160 }, { 90, 170, 230 }, { 235, 235, 210 }, { 240, 170, 70 }, { 200, 50, 40 } };
const std::vector<Rgb> FERTILE_RAMP{ { 150, 120, 90 }, { 190, 170, 100 }, { 110, 160, 70 }, { 40, 120, 50 } };
const std::vector<Rgb> WET_RAMP{ { 170, 120, 60 }, { 220, 210, 150 }, { 110, 180, 120 }, { 40, 120, 170 }, { 20, 60, 140 } };
const std::vector<Rgb> LAND_RAMP{ { 70, 120, 60 }, { 160, 150, 90 }, { 130, 110, 95 }, { 245, 245, 245 } };
const std::vector<Rgb> SEA_RAMP{ { 10, 30, 70 }, { 50, 110, 170 } };
const Rgb PROVINCE_COLOURS[] = { { 150, 140, 128 }, { 176, 84, 70 }, { 214, 142, 62 }, { 92, 172, 160 }, { 126, 172, 92 }, { 74, 52, 52 }, { 44, 74, 132 }, { 196, 192, 184 }, { 70, 72, 80 } };
const Rgb BOUNDARY_COLOURS[] = { { 0, 0, 0 }, { 236, 52, 44 }, { 52, 214, 236 }, { 236, 214, 52 } };
const Rgb DRY_SEA{ 24, 40, 70 }, WET_SEA{ 24, 50, 90 }, UNEXPLORED{ 10, 18, 24 };

Rgb mix(const Rgb &a, const Rgb &b, float t)
{
	return { a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t };
}

Rgb ramp(const std::vector<Rgb> &stops, float t)
{
	const float s = std::clamp(t, 0.f, 1.f) * (stops.size() - 1);
	const size_t i = std::min(stops.size() - 2, (size_t)s);
	return mix(stops[i], stops[i + 1], s - i);
}

Rgb currentColour(float current, bool sea)
{
	const float a = std::clamp(current / 0.25f, -1.f, 1.f), base = sea ? 60.f : 140.f;
	return { base + 180 * std::max(0.f, a) - 30 * std::max(0.f, -a), base - 30 * std::abs(a),
		base + 180 * std::max(0.f, -a) - 30 * std::max(0.f, a) };
}

Rgb seasonColour(float season, float wet)
{
	const float amount = std::clamp(0.6f + wet * 0.5f, 0.35f, 1.f);
	// only a strong season shows its colour: weak ones stay near grey
	const float s = (season < 0 ? -1.f : 1.f) * std::max(0.f, std::abs(season) - 0.25f) / 0.75f;
	return { (150 + 100 * std::max(0.f, s) - 90 * std::max(0.f, -s)) * amount, (150 - 70 * std::abs(s)) * amount,
		(150 + 100 * std::max(0.f, -s) - 90 * std::max(0.f, s)) * amount };
}

Rgb plateColour(u32 plate, bool sea)
{
	const u32 k = (plate + 1) * 2654435761u;
	const float t = sea ? 0.45f : 0.85f;
	return { (60 + ((k >> 8) % 160)) * t, (60 + ((k >> 16) % 160)) * t, (60 + ((k >> 24) % 160)) * t };
}

float real(fix v) { return v / (float)ONE; }

// samples a lattice step apart (of the world across): finer than the finest tile's cells
constexpr int LATTICE = 1 << 15;
constexpr size_t MOST_SAMPLES = 1 << 20;

} // namespace

std::optional<Overlay> overlayNamed(const std::string &name)
{
	static const std::pair<const char *, Overlay> NAMES[] = {
		{ "biomes", Overlay::Biomes }, { "elevation", Overlay::Elevation }, { "temperature", Overlay::Temperature },
		{ "moisture", Overlay::Moisture }, { "seasons", Overlay::Seasons }, { "fertility", Overlay::Fertility },
		{ "currents", Overlay::Currents }, { "plates", Overlay::Plates }, { "geology", Overlay::Geology },
	};
	for (const auto &[n, o] : NAMES)
		if (name == n)
			return o;
	return std::nullopt;
}

Painter::Painter(Opts opts) : m_opts(std::move(opts))
{
	if (!m_opts.world && !m_opts.image)
		throw BaseException("planetarium: a painter needs its world or an image");
	if (m_opts.image && m_opts.image->rgba.size() != (size_t)m_opts.image->width * m_opts.image->height * 4)
		throw BaseException("planetarium: a layer's pixels don't fill its size");
	if (m_opts.explored && m_opts.explored->size() * 8 != (size_t)m_opts.grid * m_opts.grid)
		throw BaseException("planetarium: an explored mask fills its grid");
}

const Painter::Sample &Painter::sample(fix u, fix v)
{
	// on the lattice: a vertex shared by tiles (and cells) is one sample
	const u32 i = (u32)(((int64_t)u * LATTICE) >> FRAC) & (LATTICE - 1), j = (u32)std::clamp<int64_t>(((int64_t)v * LATTICE) >> FRAC, 0, LATTICE - 1);
	const u64 key = (u64)i << 32 | j;
	if (auto found = m_samples.find(key); found != m_samples.end())
		return found->second;
	if (m_samples.size() >= MOST_SAMPLES)
		m_samples.clear();
	const World &w = *m_opts.world;
	const fix lu = ratio(2 * i + 1, 2 * LATTICE), lv = ratio(2 * j + 1, 2 * LATTICE);
	const std::optional<fix> date = m_opts.date ? std::optional<fix>((fix)(*m_opts.date * ONE)) : std::nullopt;
	const Grid &g = w.grid();
	const Place at = World::place(lu, lv);
	Sample s{ w.column(at, w.sunlight(at, date)), w.between(g.fertility, at), w.between(g.sea_heat, at), w.cell(at) };
	return m_samples.emplace(key, s).first->second;
}

video::SColor Painter::colour(video::SColor own, const far::Tiling::Vec &at)
{
	// where it is round the body, as the world model lays it out
	const double r = std::sqrt(at.x * at.x + at.y * at.y + at.z * at.z);
	const double lat = std::asin(std::clamp(at.y / r, -1.0, 1.0)), lon = std::atan2(at.z, at.x);
	const double u = lon / (2 * M_PI) + 0.5, v = 0.5 - lat / M_PI;
	// the land's own light and shade, so its relief still shows
	const float shade = 0.7f + 0.3f * std::max({ own.getRed(), own.getGreen(), own.getBlue() }) / 255.f;
	if (m_opts.image) {
		// between the image's pixels, so it's smooth however close
		const Opts::Image &im = *m_opts.image;
		const double x = u * im.width - 0.5, y = std::clamp(v * im.height - 0.5, 0.0, im.height - 1.0);
		const int x0 = (int)std::floor(x), y0 = (int)y, y1 = std::min<int>(y0 + 1, im.height - 1);
		const float fx = (float)(x - x0), fy = (float)(y - y0);
		const auto at = [&](int px, int py, int k) {
			return (float)im.rgba[((size_t)py * im.width + (u32)((px % (int)im.width + im.width) % im.width)) * 4 + k];
		};
		float c[3];
		for (int k = 0; k < 3; k++)
			c[k] = (at(x0, y0, k) * (1 - fx) + at(x0 + 1, y0, k) * fx) * (1 - fy) + (at(x0, y1, k) * (1 - fx) + at(x0 + 1, y1, k) * fx) * fy;
		return video::SColor(255, (u32)(c[0] * shade), (u32)(c[1] * shade), (u32)(c[2] * shade));
	}
	const Sample &s = sample((fix)(u * ONE), (fix)(v * ONE));
	const Column &c = s.column;
	const bool sea = c.elevation < 0;
	// a liquid's top on land is a lake or a river: the water overlays keep it the land's water
	const bool water = own.getAlpha() == 0;
	const Grid &g = m_opts.world->grid();
	Rgb colour;
	switch (m_opts.overlay) {
	case Overlay::Biomes: colour = BIOME_COLOURS[(int)c.biome]; break;
	case Overlay::Elevation: {
		const float e = real(c.elevation);
		colour = sea ? ramp(SEA_RAMP, 1 + e * 1.4f) : ramp(LAND_RAMP, e * 1.3f);
		break;
	}
	case Overlay::Temperature: colour = ramp(HEAT_RAMP, (real(c.heat) + 1) / 2); break;
	case Overlay::Moisture: colour = sea ? WET_SEA : ramp(WET_RAMP, (real(c.wet) + 1) / 2); break;
	// red: rain in summer (monsoon), blue: in winter (Mediterranean), grey: all year
	case Overlay::Seasons: colour = sea ? DRY_SEA : seasonColour(real(c.season), real(c.wet)); break;
	case Overlay::Fertility: colour = sea ? DRY_SEA : ramp(FERTILE_RAMP, real(s.fertility)); break;
	// red where the currents warm the sea and coast, blue where they chill it
	case Overlay::Currents: colour = currentColour(real(s.current), sea); break;
	case Overlay::Plates: {
		const bool edge = g.tectonics.distance[s.cell] == 0 && (int)g.tectonics.boundary[s.cell] != 0;
		colour = edge ? BOUNDARY_COLOURS[(int)g.tectonics.boundary[s.cell]] : plateColour(g.tectonics.plate[s.cell], sea);
		break;
	}
	case Overlay::Geology: colour = PROVINCE_COLOURS[(int)g.province[s.cell]]; break;
	}
	const bool waterShown = m_opts.overlay == Overlay::Biomes || m_opts.overlay == Overlay::Elevation;
	if (water && waterShown && !sea)
		return own;
	if (m_opts.explored) {
		const u32 row = std::min(m_opts.grid - 1, (u32)(v * m_opts.grid)), col = std::min(m_opts.grid - 1, (u32)(u * m_opts.grid));
		const u32 bit = row * m_opts.grid + col;
		if (!((*m_opts.explored)[bit >> 3] & (1 << (bit & 7))))
			colour = UNEXPLORED;
	}
	return video::SColor(255, (u32)std::min(255.f, colour[0] * shade), (u32)std::min(255.f, colour[1] * shade),
			(u32)std::min(255.f, colour[2] * shade));
}

} // namespace planetarium
