// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "air/tables.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace air {

namespace {

const double GROUND_ALBEDO = 0.3;
const int TRANSMIT_STEPS = 40, SKY_STEPS = 32, SCATTER_STEPS = 20, SCATTER_DIRECTIONS = 64;

struct Local
{
	Rgb rayleigh, particle, extinction;
};

Local localAt(const Medium &m, double h)
{
	h = std::max(h, 0.0);
	const double air = std::exp(-h / m.rayleigh_height), dust = std::exp(-h / m.particle_height);
	const double ozone = std::max(0.0, 1 - std::abs(h - m.ozone_at) / m.ozone_width);
	Local l;
	for (int c = 0; c < 3; c++) {
		l.rayleigh[c] = m.rayleigh[c] * air;
		l.particle[c] = m.particle_scatter[c] * dust;
		l.extinction[c] = l.rayleigh[c] + l.particle[c] + m.particle_absorb[c] * dust + m.gas_absorb[c] * air + m.ozone[c] * ozone;
	}
	return l;
}

// How far along a ray from radius r at cosine mu it meets a sphere; negative: it doesn't.
double hit(double r, double mu, double sphere)
{
	const double d = r * r * (mu * mu - 1) + sphere * sphere;
	if (d < 0)
		return -1;
	const double root = std::sqrt(d);
	const double near = -r * mu - root;
	return near >= 0 ? near : -r * mu + root;
}

// A ray's way out of the air, or to the ground. Whether it ends on the ground.
double lengthOf(const Medium &m, double r, double mu, bool &ground)
{
	ground = false;
	if (mu < 0 && r * r * (mu * mu - 1) + m.radius * m.radius >= 0) {
		const double t = -r * mu - std::sqrt(r * r * (mu * mu - 1) + m.radius * m.radius);
		if (t >= 0) {
			ground = true;
			return t;
		}
	}
	return std::max(hit(r, mu, m.top), 0.0);
}

double heightAlong(double r, double mu, double t, double radius)
{
	return std::sqrt(r * r + t * t + 2 * r * t * mu) - radius;
}

double rayleighPhase(double c) { return 3 / (16 * M_PI) * (1 + c * c); }
double particlePhase(double c, double g)
{
	const double k = 3 / (8 * M_PI) * (1 - g * g) / (2 + g * g);
	return k * (1 + c * c) / std::pow(std::max(1 + g * g - 2 * g * c, 1e-6), 1.5);
}

Vec unit(const Vec &v)
{
	const double l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	if (l <= 0)
		throw std::invalid_argument("air: a direction with no length");
	return { v.x / l, v.y / l, v.z / l };
}

double dot(const Vec &a, const Vec &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

} // namespace

Tables::Tables(const Medium &medium) : m_medium(medium)
{
	if (medium.none())
		throw std::invalid_argument("air::Tables: a body with no air");
	const Medium &m = m_medium;

	// Thick air: the paper's series is for few scatterings, and under tens of
	// them it goes black. There light diffuses, by the two-stream solution, kept
	// from the thin air's sums by how thick this is (it fades in over τ 3 to 10).
	double thick = 0;
	for (int c = 0; c < 3; c++) {
		const double rayleigh = m.rayleigh[c] * m.rayleigh_height, particle = m.particle_scatter[c] * m.particle_height;
		const double scattered = rayleigh + particle;
		const double absorbed = m.particle_absorb[c] * m.particle_height + m.gas_absorb[c] * m.rayleigh_height
				+ m.ozone[c] * m.ozone_width;
		const double total = scattered + absorbed;
		const double kept = total > 0 ? scattered / total : 1;
		const double g = scattered > 0 ? particle * m.particle_g / scattered : 0;
		m_diffuse[c] = std::exp(-std::sqrt(3 * (1 - kept) * (1 - kept * g)) * total) / (1 + 0.75 * scattered * (1 - g));
		if (c == 1)
			thick = std::clamp((total - 3) / 7, 0.0, 1.0);
	}
	for (double &d : m_diffuse)
		d *= thick * thick * (3 - 2 * thick);

	// Sunlight surviving. Rows and columns by Bruneton's mapping, which spends
	// its samples near the horizon, where the light changes fastest.
	m_transmit.resize((size_t)TRANSMIT_W * TRANSMIT_H);
	const double reach = std::sqrt(m.top * m.top - m.radius * m.radius);
	for (int y = 0; y < TRANSMIT_H; y++) {
		const double rho = reach * (y + 0.5) / TRANSMIT_H;
		const double r = std::sqrt(rho * rho + m.radius * m.radius);
		const double d_min = m.top - r, d_max = rho + reach;
		for (int x = 0; x < TRANSMIT_W; x++) {
			const double d = d_min + (x + 0.5) / TRANSMIT_W * (d_max - d_min);
			const double mu = std::clamp((reach * reach - rho * rho - d * d) / (2 * r * d), -1.0, 1.0);
			Rgb depth{};
			for (int i = 0; i < TRANSMIT_STEPS; i++) {
				const double t = d * (i + 0.5) / TRANSMIT_STEPS;
				const Local l = localAt(m, heightAlong(r, mu, t, m.radius));
				for (int c = 0; c < 3; c++)
					depth[c] += l.extinction[c] * d / TRANSMIT_STEPS;
			}
			for (int c = 0; c < 3; c++)
				m_transmit[(size_t)y * TRANSMIT_W + x][c] = std::exp(-depth[c]);
		}
	}

	// Later scattering (the paper's section 5.5): from each place the light of
	// the second scattering, averaged all round, and the share of light each
	// scattering hands on; the rest of the series is a geometric sum of that.
	m_scatter.resize((size_t)SCATTER_SIZE * SCATTER_SIZE);
	const double height_span = m.top - m.radius;
	std::vector<Vec> directions(SCATTER_DIRECTIONS);
	for (int i = 0; i < SCATTER_DIRECTIONS; i++) { // spread evenly over the sphere
		const double y = 1 - 2 * (i + 0.5) / SCATTER_DIRECTIONS, a = i * M_PI * (3 - std::sqrt(5.0));
		const double s = std::sqrt(1 - y * y);
		directions[i] = { s * std::cos(a), y, s * std::sin(a) };
	}
	for (int y = 0; y < SCATTER_SIZE; y++) {
		const double f = (y + 0.5) / SCATTER_SIZE;
		const double r = m.radius + height_span * f * f;
		for (int x = 0; x < SCATTER_SIZE; x++) {
			const double cos_sun = 2 * (x + 0.5) / SCATTER_SIZE - 1;
			const Vec sun{ std::sqrt(1 - cos_sun * cos_sun), cos_sun, 0 };
			Rgb second{}, handed{};
			for (const Vec &v : directions) {
				bool ground;
				const double length = lengthOf(m, r, v.y, ground);
				Rgb seen{ 1, 1, 1 };
				const double dt = length / SCATTER_STEPS;
				Rgb light{}, hand{};
				for (int i = 0; i < SCATTER_STEPS; i++) {
					const double t = (i + 0.5) * dt;
					const double ri = std::sqrt(r * r + t * t + 2 * r * t * v.y);
					const Local l = localAt(m, ri - m.radius);
					const Vec up{ v.x * t / ri, (r + t * v.y) / ri, v.z * t / ri };
					const Rgb lit = transmittance(ri - m.radius, dot(up, sun));
					for (int c = 0; c < 3; c++) {
						const double scatter = l.rayleigh[c] + l.particle[c];
						const double ext = std::max(l.extinction[c], 1e-30);
						const double step = std::exp(-ext * dt);
						// the exact sum over the step, so thick air doesn't blow up
						const double share = (1 - step) / ext;
						light[c] += seen[c] * scatter * lit[c] / (4 * M_PI) * share;
						hand[c] += seen[c] * scatter * share;
						seen[c] *= step;
					}
				}
				if (ground) {
					const double ground_r = m.radius;
					const Vec up{ v.x * length / ground_r, (r + length * v.y) / ground_r, v.z * length / ground_r };
					const double cos_ground = dot(up, sun);
					if (cos_ground > 0) {
						const Rgb lit = transmittance(0, cos_ground);
						for (int c = 0; c < 3; c++)
							light[c] += seen[c] * lit[c] * cos_ground * GROUND_ALBEDO / M_PI;
					}
				}
				for (int c = 0; c < 3; c++) {
					second[c] += light[c] / SCATTER_DIRECTIONS;
					handed[c] += hand[c] / SCATTER_DIRECTIONS;
				}
			}
			Rgb &out = m_scatter[(size_t)y * SCATTER_SIZE + x];
			for (int c = 0; c < 3; c++)
				out[c] = second[c] / (1 - std::min(handed[c], 0.999));
		}
	}
}

Rgb Tables::transmittance(double height, double cos_sun) const
{
	const Medium &m = m_medium;
	const double r = std::clamp(m.radius + height, m.radius, m.top);
	// below the horizon, the planet's in the way
	if (cos_sun < 0 && r * r * (cos_sun * cos_sun - 1) + m.radius * m.radius >= 0)
		return { 0, 0, 0 };
	const double reach = std::sqrt(m.top * m.top - m.radius * m.radius);
	const double rho = std::sqrt(std::max(r * r - m.radius * m.radius, 0.0));
	const double d = std::max(hit(r, cos_sun, m.top), 0.0);
	const double d_min = m.top - r, d_max = rho + reach;
	const double u = d_max > d_min ? (d - d_min) / (d_max - d_min) : 0;
	const double fx = std::clamp(u * TRANSMIT_W - 0.5, 0.0, TRANSMIT_W - 1.0);
	const double fy = std::clamp(rho / reach * TRANSMIT_H - 0.5, 0.0, TRANSMIT_H - 1.0);
	const int x = (int)fx, y = (int)fy, x1 = std::min(x + 1, TRANSMIT_W - 1), y1 = std::min(y + 1, TRANSMIT_H - 1);
	const double a = fx - x, b = fy - y;
	const auto at = [&](int ix, int iy) -> const Rgb & { return m_transmit[(size_t)iy * TRANSMIT_W + ix]; };
	Rgb out;
	for (int c = 0; c < 3; c++)
		out[c] = (at(x, y)[c] * (1 - a) + at(x1, y)[c] * a) * (1 - b) + (at(x, y1)[c] * (1 - a) + at(x1, y1)[c] * a) * b;
	return out;
}

Rgb Tables::multiple(double height, double cos_sun) const
{
	const Medium &m = m_medium;
	const double f = std::sqrt(std::clamp(height / (m.top - m.radius), 0.0, 1.0));
	const double fx = std::clamp((cos_sun + 1) / 2 * SCATTER_SIZE - 0.5, 0.0, SCATTER_SIZE - 1.0);
	const double fy = std::clamp(f * SCATTER_SIZE - 0.5, 0.0, SCATTER_SIZE - 1.0);
	const int x = (int)fx, y = (int)fy, x1 = std::min(x + 1, SCATTER_SIZE - 1), y1 = std::min(y + 1, SCATTER_SIZE - 1);
	const double a = fx - x, b = fy - y;
	const auto at = [&](int ix, int iy) -> const Rgb & { return m_scatter[(size_t)iy * SCATTER_SIZE + ix]; };
	Rgb out;
	for (int c = 0; c < 3; c++)
		out[c] = (at(x, y)[c] * (1 - a) + at(x1, y)[c] * a) * (1 - b) + (at(x, y1)[c] * (1 - a) + at(x1, y1)[c] * a) * b;
	return out;
}

Rgb Tables::sky(double height, const Vec &view_in, const Vec &sun_in) const
{
	const Medium &m = m_medium;
	const Vec view = unit(view_in), sun = unit(sun_in);
	const double r = m.radius + std::max(height, 0.0);
	bool ground;
	const double length = lengthOf(m, r, view.y, ground);
	const double c = dot(view, sun);
	const double rp = rayleighPhase(c), pp = particlePhase(c, m.particle_g);
	Rgb seen{ 1, 1, 1 }, light{};
	double from = 0;
	for (int i = 1; i <= SKY_STEPS; i++) {
		// steps bunched near the eye, where the air's thickest
		const double f = (double)i / SKY_STEPS, to = length * f * f;
		const double dt = to - from, t = (from + to) / 2;
		from = to;
		const double ri = std::sqrt(r * r + t * t + 2 * r * t * view.y);
		const Local l = localAt(m, ri - m.radius);
		const Vec up{ view.x * t / ri, (r + t * view.y) / ri, view.z * t / ri };
		const double cos_here = dot(up, sun);
		const Rgb lit = transmittance(ri - m.radius, cos_here);
		const Rgb more = multiple(ri - m.radius, cos_here);
		for (int k = 0; k < 3; k++) {
			const double ext = std::max(l.extinction[k], 1e-30);
			const double step = std::exp(-ext * dt);
			const double in = lit[k] * (l.rayleigh[k] * rp + l.particle[k] * pp) + (l.rayleigh[k] + l.particle[k]) * more[k];
			light[k] += seen[k] * in * (1 - step) / ext;
			seen[k] *= step;
		}
	}
	if (ground) {
		const Vec up{ view.x * length / m.radius, (r + length * view.y) / m.radius, view.z * length / m.radius };
		const double cos_ground = dot(up, sun);
		if (cos_ground > 0) {
			const Rgb lit = transmittance(0, cos_ground);
			for (int k = 0; k < 3; k++)
				light[k] += seen[k] * lit[k] * cos_ground * GROUND_ALBEDO / M_PI;
		}
	}
	// (from under the thick of it; what's left of the air above a height)
	const double above = std::exp(-std::max(height, 0.0) / m.rayleigh_height);
	const double cos_top = std::max(sun.y, 0.0);
	for (int k = 0; k < 3; k++) {
		light[k] += m_diffuse[k] * cos_top * above / M_PI;
		light[k] *= m.star[k];
	}
	return light;
}

Rgb Tables::through(double height, const Vec &view_in) const
{
	const Medium &m = m_medium;
	const Vec view = unit(view_in);
	const double r = m.radius + std::max(height, 0.0);
	bool ground;
	const double length = lengthOf(m, r, view.y, ground);
	if (ground)
		return { 0, 0, 0 };
	Rgb depth{};
	for (int i = 0; i < SKY_STEPS; i++) {
		const double t = length * (i + 0.5) / SKY_STEPS;
		const Local l = localAt(m, heightAlong(r, view.y, t, m.radius));
		for (int k = 0; k < 3; k++)
			depth[k] += l.extinction[k] * length / SKY_STEPS;
	}
	return { std::exp(-depth[0]), std::exp(-depth[1]), std::exp(-depth[2]) };
}

double realElevation(double e, double drawn_dip, double real_dip)
{
	const double right = M_PI / 2;
	if (e >= -drawn_dip)
		return (e + drawn_dip) / (right + drawn_dip) * (right + real_dip) - real_dip;
	return -real_dip + (e + drawn_dip) / (right - drawn_dip) * (right - real_dip);
}

static double shown(double v)
{
	v = 1 - std::exp(-v);
	v = v <= 0.0031308 ? 12.92 * v : 1.055 * std::pow(v, 1 / 2.4) - 0.055;
	return std::clamp(v, 0.0, 1.0);
}

SkyImage skyImage(const Tables &tables, double height, double elevation, double light, const Rgb &tint)
{
	const Vec sun{ 0, std::sin(elevation), std::cos(elevation) };
	SkyImage image;
	image.rgba.resize((size_t)SkyImage::W * SkyImage::H * 4);
	image.horizon = { 0, 0, 0 };
	const int horizon_row = SkyImage::H / 2 - 1; // (rows run from up: just above the middle)
	for (int y = 0; y < SkyImage::H; y++) {
		const double v = 1 - (y + 0.5) / SkyImage::H; // 0 down .. 1 up
		const double side = 2 * v - 1;
		const double angle = (side < 0 ? -1 : 1) * side * side * M_PI / 2;
		const double cos_up = std::cos(angle), sin_up = std::sin(angle);
		for (int x = 0; x < SkyImage::W; x++) {
			const double round = (x + 0.5) / SkyImage::W * M_PI;
			const Vec view{ cos_up * std::sin(round), sin_up, cos_up * std::cos(round) };
			const Rgb sky = tables.sky(height, view, sun);
			const Rgb through = tables.through(height, view);
			unsigned char *p = &image.rgba[((size_t)y * SkyImage::W + x) * 4];
			Rgb out;
			for (int c = 0; c < 3; c++) {
				out[c] = shown(sky[c] * light * tint[c]);
				p[c] = (unsigned char)std::lround(out[c] * 255);
			}
			if (y == horizon_row)
				for (int c = 0; c < 3; c++)
					image.horizon[c] += out[c] / SkyImage::W;
			// a bright sky drowns what's behind it, whatever gets through
			const double bright = (out[0] + out[1] + out[2]) / 3;
			const double drown = std::clamp(1 - bright / 0.25, 0.0, 1.0);
			p[3] = (unsigned char)std::lround((through[0] + through[1] + through[2]) / 3 * drown * 255);
		}
	}
	const Rgb lit = tables.transmittance(height, sun.y);
	const Rgb &star = tables.medium().star;
	for (int c = 0; c < 3; c++)
		image.sun[c] = shown(lit[c] * star[c] * tint[c] * light); // (very bright, unless the air's taken it)
	return image;
}

Rgb sunTint(const Tables &tables, double height, double elevation)
{
	const Rgb noon = tables.transmittance(height, 1);
	const Rgb lit = tables.transmittance(height, std::sin(elevation));
	const Rgb &star = tables.medium().star;
	Rgb direct;
	double most = 1e-9;
	for (int c = 0; c < 3; c++) {
		direct[c] = lit[c] / std::max(noon[c], 1e-9) * star[c];
		most = std::max(most, direct[c]);
	}
	const Rgb DUSK = { 0.55, 0.6, 0.85 };
	// from just under the horizon to just over it
	const double f = std::clamp((elevation + 0.05) / 0.07, 0.0, 1.0);
	const double day = f * f * (3 - 2 * f);
	Rgb out;
	for (int c = 0; c < 3; c++)
		out[c] = DUSK[c] * (1 - day) + (0.75 * direct[c] / most + 0.25) * day;
	return out;
}

RimImage rimImage(const Tables &tables, double reach, double light)
{
	const Medium &m = tables.medium();
	RimImage image;
	image.rgb.resize((size_t)RimImage::W * RimImage::H * 3);
	for (int y = 0; y < RimImage::H; y++) {
		const double height = (double)y / (RimImage::H - 1) * reach * (m.top - m.radius);
		for (int x = 0; x < RimImage::W; x++) {
			const double up = 2 * (x + 0.5) / RimImage::W - 1;
			const double across = std::sqrt(1 - up * up);
			const Rgb sky = tables.sky(height, { 0, 0, 1 }, { across, up, 0 });
			for (int c = 0; c < 3; c++)
				// (a sight line edge on crosses the air both ways from where it's lowest)
				image.rgb[((size_t)y * RimImage::W + x) * 3 + c] = (unsigned char)std::lround(shown(2 * sky[c] * light) * 255);
		}
	}
	return image;
}

} // namespace air
