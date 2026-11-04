// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "milkyway/milkyway.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <thread>

namespace milkyway {

namespace {

// All lengths in kiloparsecs. A galaxy like ours, us a little over halfway out.
constexpr float SUN_R = 8.0f, SUN_Z = 0.02f;
constexpr float NEAREST = 0.03f, FARTHEST = 30.0f; // the ray, from the local bubble out
constexpr int STEPS = 56;                          // spaced evenly in log distance: near dust is big on the sky
constexpr float PITCH = 0.22f;                     // how tightly the arms wind (radians)
constexpr float PI = 3.14159265f;
constexpr float BLACK = 0.008f;                    // the picture's black point

struct V3
{
	float x, y, z;
};
inline V3 operator+(V3 a, V3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline V3 operator*(V3 a, float k) { return { a.x * k, a.y * k, a.z * k }; }
inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(V3 a, V3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
inline V3 unit(V3 a) { return a * (1 / std::sqrt(dot(a, a))); }

struct Lattice
{
	float values[4096];
	Lattice()
	{
		uint32_t seed = 0x51ed27;
		for (float &v : values) {
			seed = seed * 1664525u + 1013904223u;
			v = (seed >> 8) / 16777216.0f;
		}
	}
	float at(int x, int y, int z) const
	{
		uint32_t h = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^ (uint32_t)z * 83492791u;
		return values[(h ^ (h >> 15)) & 4095];
	}
};
const Lattice LATTICE;

inline float noise(V3 p)
{
	const int x = (int)std::floor(p.x), y = (int)std::floor(p.y), z = (int)std::floor(p.z);
	const float fx = p.x - x, fy = p.y - y, fz = p.z - z;
	const float u = fx * fx * (3 - 2 * fx), v = fy * fy * (3 - 2 * fy), w = fz * fz * (3 - 2 * fz);
	const auto mix = [](float a, float b, float t) { return a + (b - a) * t; };
	const Lattice &l = LATTICE;
	return mix(mix(mix(l.at(x, y, z), l.at(x + 1, y, z), u), mix(l.at(x, y + 1, z), l.at(x + 1, y + 1, z), u), v),
			mix(mix(l.at(x, y, z + 1), l.at(x + 1, y, z + 1), u), mix(l.at(x, y + 1, z + 1), l.at(x + 1, y + 1, z + 1), u), v), w);
}

inline float fbm(V3 p, int octaves)
{
	float sum = 0, weight = 0.5f, total = 0;
	for (int i = 0; i < octaves; i++) {
		sum += noise(p) * weight;
		total += weight;
		weight *= 0.5f;
		p = p * 2.03f + V3{ 7.1f, 3.3f, 5.7f };
	}
	return sum / total;
}

inline float smooth(float from, float to, float x)
{
	const float t = std::clamp((x - from) / (to - from), 0.0f, 1.0f);
	return t * t * (3 - 2 * t);
}

inline float sech2(float x)
{
	const float c = std::cosh(std::min(x, 12.0f));
	return 1 / (c * c);
}

// How much of an arm is here, 0..1: four logarithmic spirals.
inline float arms(float x, float y, float r)
{
	const float theta = std::atan2(y, x);
	const float phase = 4 * (theta - std::log(std::max(r, 0.5f) / SUN_R) / std::tan(PITCH));
	const float wave = 0.5f + 0.5f * std::cos(phase);
	return wave * wave * wave;
}

struct Rgb
{
	float r, g, b;
};

struct Seen
{
	Rgb stars, gas;
};

Seen march(V3 way)
{
	const float ratio = std::pow(FARTHEST / NEAREST, 1.0f / STEPS);
	float s0 = NEAREST;
	Rgb stars{ 0, 0, 0 }, gas{ 0, 0, 0 };
	float depth = 0; // dust passed, at blue-green
	for (int i = 0; i < STEPS; i++) {
		const float s1 = s0 * ratio, ds = s1 - s0, s = (s0 + s1) / 2;
		s0 = s1;
		const V3 p{ SUN_R + way.x * s, way.y * s, SUN_Z + way.z * s };
		const float az = std::abs(p.z);
		if (az > 4)
			break; // out of it, and going further
		const float R = std::sqrt(p.x * p.x + p.y * p.y);
		if (R > 22)
			continue;
		// what dust has been passed dims blue most: its edges redden
		const float tr = std::exp(-depth * 0.8f), tg = std::exp(-depth), tb = std::exp(-depth * 1.25f);

		// old stars: the disc, a thick disc, the bar and bulge, warm
		const float disc = std::exp(-R / 2.6f) * sech2(az / 0.15f);
		const float thick = 0.006f * std::exp(-R / 3.6f) * std::exp(-az / 0.9f);
		const float bar = (p.x * 0.8f + p.y * 0.6f), across = (-p.x * 0.6f + p.y * 0.8f);
		const float box = std::sqrt(bar * bar / 2.2f + across * across + p.z * p.z / 0.3f);
		const float bulge = 5.0f * std::exp(-box / 0.5f);
		const float old = (disc + thick) * 3.2f;
		stars.r += (old * 1.0f + bulge * 1.0f) * tr * ds;
		stars.g += (old * 0.84f + bulge * 0.76f) * tg * ds;
		stars.b += (old * 0.74f + bulge * 0.56f) * tb * ds;

		if (az > 0.9f)
			continue; // the rest is thin
		const float arm = arms(p.x, p.y, R);
		// a warped place, so what's made of it is filaments, not blobs
		const V3 q{ p.x * 3.2f, p.y * 3.2f, p.z * 9.0f };
		const float wx = fbm(q + V3{ 4.1f, 1.7f, 9.2f }, 3) - 0.5f, wy = fbm(q + V3{ 8.3f, 2.9f, 1.3f }, 3) - 0.5f;
		const V3 warped = q + V3{ wx * 2.4f, wy * 2.4f, (wx - wy) * 1.2f };

		// young stars: blue, in the arms, in clusters
		const float cluster = fbm(warped * 1.3f + V3{ 3, 7, 1 }, 3);
		const float young = 3.2f * std::exp(-R / 4.5f) * sech2(az / 0.12f) * arm * smooth(0.3f, 0.7f, cluster);
		stars.r += young * 0.50f * tr * ds;
		stars.g += young * 0.62f * tg * ds;
		stars.b += young * 1.25f * tb * ds;

		// hydrogen lit by them: pink, where they're thickest
		const float cloud = smooth(0.50f, 0.68f, cluster) * arm;
		const float lit = 5.5f * std::exp(-R / 5.0f) * std::exp(-az / 0.06f) * cloud;
		gas.r += lit * 1.0f * tr * ds;
		gas.g += lit * 0.20f * tg * ds;
		gas.b += lit * 0.48f * tb * ds;

		// dust: a thin layer, thickest on the arms, in clumps with gaps between
		if (az < 0.5f) {
			const float clump = fbm(warped, 6);
			const float grain = smooth(0.44f, 0.62f, clump);
			depth += 13.0f * std::exp(-R / 5.5f) * smooth(0.6f, 2.5f, R) * std::exp(-az / 0.075f) * (0.35f + 1.3f * arm) * grain * ds;
		}
	}
	return { stars, gas };
}

inline uint32_t mixBits(uint32_t x, uint32_t y)
{
	uint32_t h = x * 374761393u + y * 668265263u;
	h = (h ^ (h >> 13)) * 1274126177u;
	return h ^ (h >> 16);
}

} // namespace

Picture bake(const Opts &opts)
{
	const V3 pole = unit({ opts.pole.x, opts.pole.y, opts.pole.z });
	V3 centre{ opts.centre.x, opts.centre.y, opts.centre.z };
	centre = unit(centre + pole * -dot(centre, pole));
	const V3 side = cross(pole, centre);

	Picture out{ opts.width, opts.height, std::vector<uint8_t>((size_t)opts.width * opts.height * 3) };
	std::atomic<uint32_t> row{ 0 };
	const auto work = [&]() {
		for (uint32_t y; (y = row.fetch_add(1)) < opts.height;) {
			const float latitude = (0.5f - (y + 0.5f) / opts.height) * PI;
			for (uint32_t x = 0; x < opts.width; x++) {
				const float longitude = (x + 0.5f) / opts.width * 2 * PI;
				const V3 d{ std::cos(latitude) * std::cos(longitude), std::sin(latitude), std::cos(latitude) * std::sin(longitude) };
				// in the galaxy's axes: its middle's along −x from us
				const Seen seen = march({ -dot(d, centre), dot(d, side), dot(d, pole) });
				const float dither = (mixBits(y, x + 99991) / 4294967296.0f - 0.5f) / 255;
				const auto shown = [&](float stars, float gas) {
					// the sky's dim and only its middle bright: a long shoulder, display gamma
					// (the faintest light goes to black: gamma would lift it into a grey sky)
					const float v = std::max(1 - std::exp(-(stars + gas) * opts.exposure) - BLACK, 0.0f) / (1 - BLACK);
					return (uint8_t)(std::clamp(std::pow(v, 1 / 1.5f) + dither, 0.0f, 1.0f) * 255 + 0.5f);
				};
				uint8_t *to = &out.rgb[((size_t)y * opts.width + x) * 3];
				to[0] = shown(seen.stars.r, seen.gas.r);
				to[1] = shown(seen.stars.g, seen.gas.g);
				to[2] = shown(seen.stars.b, seen.gas.b);
			}
		}
	};
	std::vector<std::thread> threads;
	for (unsigned i = 0; i < std::max(1u, std::thread::hardware_concurrency()); i++)
		threads.emplace_back(work);
	for (std::thread &t : threads)
		t.join();
	return out;
}

} // namespace milkyway
