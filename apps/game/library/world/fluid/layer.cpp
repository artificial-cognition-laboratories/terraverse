// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/fluid/fluid.h"
#include <algorithm>
#include <cmath>

namespace fluid {

static const double STEFAN = 5.670374e-8;
static const double ALBEDO = 0.3;
static const double FEEDBACK = 0.4;      // share of 4σT³ left after water vapour and cloud feedbacks (Budyko's ~2 W/m²K on Earth)
static const double TRANSPORT = 0.8;     // heat the whole fluid carries poleward, as a share of that feedback (Budyko–Sellers)
static const double EXCHANGE = 1.3e-3;   // bulk transfer coefficient for evaporation
static const double MOIST_SHARE = 0.3;   // the share of the column's mass that holds its vapour
static const double CRITICAL = 0.85;     // mean relative humidity where part of a cell starts to condense
static const double LIFTING = 6 * 3600;  // s: how long rising air rises before it rains out
static const double ALOFT_SHARE = 0.2;   // of the latent heat, what stays in the air near the ground (most warms the air aloft, where it condenses)
static const double BOUNDARY = 8000;     // m: the low air whose convergence feeds rising air
static const double CONDENSE = 3 * 3600; // s: how fast excess vapour rains out
static const double SOIL = 150;          // kg/m²: what a full bucket of ground holds
static const double EDDY = 1.5e6;        // m²/s: heat carried poleward by the eddies the grid can't hold
static const double COUPLE = 86400;      // s: the air follows its surface
// How slowly the surface follows the seasons, as a share of the year (a
// relaxation time τ): a season's swing is damped to 1/√(1 + (2πτ)²) and
// arrives atan(2πτ) late — land about a month behind the sun, the sea's
// mixed layer about two, at half the swing (Earth's)
static const double SEA_INERTIA = 0.3;
static const double LAND_INERTIA = 0.1;
static const double ASH_LIFE = 5 * 86400;

Layer::Layer(LayerOpts opts) : m_opts(std::move(opts))
{
	const LayerOpts &o = m_opts;
	size_t n = (size_t)o.width * o.height;
	if (o.width < 8 || o.height < 4 || o.ground.size() != n)
		throw FluidError("fluid layer: the ground must be width × height cells (at least 8 × 4)");
	if (o.air.pressure <= 0 || o.air.gases.empty() || o.air.mean <= 0)
		throw FluidError("fluid layer: the air needs gases, a pressure and a mean temperature");
	double shares = 0, mass = 0, heat = 0;
	for (const Gas &g : o.air.gases) {
		shares += g.share;
		mass += g.share * g.substance.molar_mass;
		heat += g.share * g.substance.molar_mass * g.substance.heat_capacity;
	}
	if (std::fabs(shares - 1) > 1e-3)
		throw FluidError("fluid layer: the gases' shares must sum to 1");
	for (const Vent &v : o.vents)
		if (v.cell < 0 || (size_t)v.cell >= n)
			throw FluidError("fluid layer: a vent outside the grid");

	m_molar = mass / 1000.0;
	m_cp = heat / mass;
	m_scale = matter::GAS_CONSTANT * o.air.mean / (m_molar * o.planet.gravity);
	m_latent = o.air.condensable.vaporization * 1000.0 / (o.air.condensable.molar_mass / 1000.0);
	m_feedback = FEEDBACK * 4 * STEFAN * std::pow(o.air.mean, 3);
	m_circulation = circulate(o.planet, o.air);

	// the layer's own seasons by latitude, which an anchored cell rescales to its own
	m_annual.assign(o.height, 0);
	m_swing.assign(o.height, 0);
	for (int32_t row = 0; row < o.height; row++) {
		for (int k = 0; k < 24; k++)
			m_annual[row] += sunlit(row, k / 24.0) / 24;
		m_swing[row] = (sunlit(row, 0.25) - sunlit(row, 0.75)) / 2;
	}

	Fields &f = m_f;
	for (auto *v : { &f.t, &f.q, &f.cloud, &f.rain, &f.ts, &f.soil, &f.u, &f.v, &f.pressure, &f.ash, &f.recent, &f.normal })
		v->assign(n, 0);
	m_lift.assign(n, 0);
	for (size_t i = 0; i < n; i++) {
		f.ts[i] = f.t[i] = radiative((int32_t)i, 0);
		f.q[i] = 0.6 * saturation(f.t[i], o.air.pressure);
		f.soil[i] = o.ground[i].water ? 1 : std::clamp(o.ground[i].soil, 0.0, 1.0);
	}
}

double Layer::latitude(int32_t row) const
{
	return M_PI / 2 - (row + 0.5) * M_PI / m_opts.height;
}

double Layer::insolation(double lat, double date) const
{
	double decl = std::asin(std::sin(m_opts.planet.tilt) * std::sin(2 * M_PI * date));
	double h0 = std::acos(std::clamp(-std::tan(lat) * std::tan(decl), -1.0, 1.0));
	double q = m_opts.planet.sunlight / M_PI * (h0 * std::sin(lat) * std::sin(decl) + std::cos(lat) * std::cos(decl) * std::sin(h0));
	return std::max(q, 0.0);
}

double Layer::sunlit(int32_t row, double date) const
{
	double q = insolation(latitude(row), date);
	return m_opts.air.mean + (q - m_opts.planet.sunlight / 4) * (1 - ALBEDO) / (m_feedback * (1 + TRANSPORT));
}

double Layer::radiative(int32_t cell, double date) const
{
	const LayerOpts &o = m_opts;
	int32_t row = cell / o.width;
	double t = sunlit(row, date);
	const Ground &g = o.ground[cell];
	if (g.anchor <= 0)
		return t - std::max(g.height, 0.0) * lapse();
	// the anchor's annual mean and swing, on the layer's own seasonal curve
	double swing = m_swing[row];
	if (std::fabs(swing) < 1) // near the equator: no season to rescale
		return g.anchor + t - m_annual[row];
	return g.anchor + g.season * (t - m_annual[row]) / swing;
}

double Layer::saturation(double t, double p) const
{
	const matter::Substance &c = m_opts.air.condensable;
	t = std::clamp(t, c.triple.t * 0.5, c.critical.t - 1);
	double e = t < c.triple.t ? c.sublimationPressure(t) : c.vapourPressure(t);
	e = std::min(e, p / 2);
	return c.molar_mass / 1000.0 / m_molar * e / p;
}

void Layer::step(double dt, double date)
{
	if (dt <= 0)
		throw FluidError("fluid layer: steps must move forward");
	storm(dt, date);
	wind(date);
	// air keeps its potential temperature as it moves (what it would be
	// brought down to the datum): blown up a plateau, it arrives cooler
	double lapse = this->lapse();
	for (size_t i = 0; i < m_f.t.size(); i++)
		m_f.t[i] += lapse * std::max(m_opts.ground[i].height, 0.0);
	for (auto *field : { &m_f.t, &m_f.q, &m_f.ash })
		advect(*field, dt);
	spread(m_f.t, EDDY, dt);
	spread(m_f.q, EDDY / 2, dt);
	for (size_t i = 0; i < m_f.t.size(); i++)
		m_f.t[i] -= lapse * std::max(m_opts.ground[i].height, 0.0);
	heat(dt, date);
	moisture(dt);
	ashes(dt);
	m_time += dt;
	m_steps++;
}

void Layer::wind(double date)
{
	const LayerOpts &o = m_opts;
	double decl = std::asin(std::sin(o.planet.tilt) * std::sin(2 * M_PI * date));
	double itcz = 0.45 * decl;
	double a = o.planet.radius;
	double dy = M_PI * a / o.height;
	for (int32_t row = 0; row < o.height; row++) {
		double lat = latitude(row);
		Flow bg = flow(m_circulation, lat, itcz);
		double dx = 2 * M_PI * a * std::max(std::cos(lat), 0.05) / o.width;
		for (int32_t col = 0; col < o.width; col++) {
			int32_t i = row * o.width + col;
			double u = bg.u, v = bg.v, w = bg.w, dp = 0;
			stormWind(i, u, v, w, dp);
			// wind blowing up a slope is lifted by it
			auto land = [&](int32_t c, int32_t r) {
				r = std::clamp(r, 0, o.height - 1);
				return std::max(o.ground[r * o.width + (c + o.width) % o.width].height, 0.0);
			};
			double slope_x = (land(col + 1, row) - land(col - 1, row)) / (2 * dx);
			double slope_y = (land(col, row - 1) - land(col, row + 1)) / (2 * dy);
			w += u * slope_x + v * slope_y;
			// air warmer at the ground than above it rises by itself
			w += 0.004 * std::max(m_f.ts[i] - m_f.t[i] - 1, 0.0);
			m_f.u[i] = u;
			m_f.v[i] = v;
			m_lift[i] = w;
			m_f.pressure[i] = -bg.w / m_circulation.lift * 600 + dp;
		}
	}
}

void Layer::advect(std::vector<double> &field, double dt)
{
	const LayerOpts &o = m_opts;
	double a = o.planet.radius;
	std::vector<double> out(field.size());
	for (int32_t row = 0; row < o.height; row++) {
		double lat = latitude(row);
		double cosl = std::max(std::cos(lat), 0.05);
		for (int32_t col = 0; col < o.width; col++) {
			int32_t i = row * o.width + col;
			// where this cell's air came from
			double x = col - m_f.u[i] * dt / (a * cosl) * o.width / (2 * M_PI);
			double y = std::clamp(row + m_f.v[i] * dt / a * o.height / M_PI, 0.0, o.height - 1.0);
			int32_t x0 = (int32_t)std::floor(x), y0 = std::min((int32_t)y, o.height - 2);
			double fx = x - x0, fy = y - y0;
			auto at = [&](int32_t c, int32_t r) { return field[r * o.width + ((c % o.width) + o.width) % o.width]; };
			out[i] = (at(x0, y0) * (1 - fx) + at(x0 + 1, y0) * fx) * (1 - fy)
					+ (at(x0, y0 + 1) * (1 - fx) + at(x0 + 1, y0 + 1) * fx) * fy;
		}
	}
	field.swap(out);
}

void Layer::spread(std::vector<double> &field, double k, double dt)
{
	const LayerOpts &o = m_opts;
	double dy = M_PI * o.planet.radius / o.height;
	double r = k * dt / (dy * dy);
	int steps = std::max(1, (int)std::ceil(r / 0.2));
	r /= steps;
	std::vector<double> out(field.size());
	for (int s = 0; s < steps; s++) {
		for (int32_t row = 0; row < o.height; row++)
			for (int32_t col = 0; col < o.width; col++) {
				int32_t i = row * o.width + col;
				double up = row > 0 ? field[i - o.width] : field[i];
				double down = row < o.height - 1 ? field[i + o.width] : field[i];
				out[i] = field[i] + r * (up + down - 2 * field[i]);
			}
		field.swap(out);
	}
}

void Layer::heat(double dt, double date)
{
	const LayerOpts &o = m_opts;
	for (size_t i = 0; i < m_f.t.size(); i++) {
		double target = radiative((int32_t)i, date) - 20 * m_f.ash[i];
		double inertia = (o.ground[i].water ? SEA_INERTIA : LAND_INERTIA) * o.planet.year;
		m_f.ts[i] += (target - m_f.ts[i]) * std::min(1.0, dt / inertia);
		m_f.t[i] += (m_f.ts[i] - m_f.t[i]) * std::min(1.0, dt / COUPLE);
	}
}

void Layer::moisture(double dt)
{
	const LayerOpts &o = m_opts;
	double gas = matter::GAS_CONSTANT / m_molar;
	double lapse = 0.6 * o.planet.gravity / m_cp; // a moist parcel cools slower than a dry one
	double year = o.planet.year;
	for (size_t i = 0; i < m_f.t.size(); i++) {
		const Ground &g = o.ground[i];
		double p = o.air.pressure * std::exp(-std::max(g.height, 0.0) / m_scale);
		double column = MOIST_SHARE * p / o.planet.gravity; // kg/m² holding the vapour
		double &q = m_f.q[i], &t = m_f.t[i];

		// evaporation, from the sea freely and from the ground as its bucket allows
		double speed = std::hypot(m_f.u[i], m_f.v[i]) + 1;
		double density = p / (gas * t);
		double wet = g.water ? 1 : m_f.soil[i];
		double evaporated = density * EXCHANGE * speed * std::max(saturation(m_f.ts[i], p) - q, 0.0) * wet * dt;
		q += evaporated / column;

		// rising air is fed by low air flowing in from around it, carrying its
		// vapour; sinking air spreads out and takes it away
		// (never past what the surface under it could saturate: the inflow
		// is surface air, and nothing here takes it from the neighbours)
		double fed = q * std::exp(std::clamp(m_lift[i] * dt / BOUNDARY, -1.0, 1.0));
		q = fed > q ? std::min(fed, std::max(q, saturation(m_f.ts[i], p))) : fed;

		// rising air cools and rains out what it can't hold; sinking air dries
		double lifted = t - lapse * m_lift[i] * LIFTING;
		double holds = CRITICAL * saturation(lifted, p);
		double condensed = std::max(q - holds, 0.0) * std::min(1.0, dt / CONDENSE);
		q -= condensed;
		t += ALOFT_SHARE * m_latent * condensed * MOIST_SHARE / m_cp;
		m_f.cloud[i] = std::clamp((q / holds - 0.6) / 0.4, 0.0, 1.0);

		double fallen = condensed * column; // kg/m²
		m_f.rain[i] = fallen / dt * 3600;   // mm/h
		if (!g.water)
			m_f.soil[i] = std::clamp(m_f.soil[i] + (fallen - evaporated) / SOIL, 0.0, 1.0);
		m_f.recent[i] += (m_f.rain[i] - m_f.recent[i]) * std::min(1.0, dt / (year / 6));
		m_f.normal[i] += (m_f.rain[i] - m_f.normal[i]) * std::min(1.0, dt / year);
	}
}

void Layer::ashes(double dt)
{
	for (const Vent &v : m_opts.vents)
		m_f.ash[v.cell] = std::min(1.0, m_f.ash[v.cell] + v.strength * dt / 86400);
	for (size_t i = 0; i < m_f.ash.size(); i++)
		m_f.ash[i] *= std::exp(-dt / ASH_LIFE - m_f.rain[i] * dt / 3600 * 0.05); // settles, and rain washes it out
}

const matter::Substance &pickCondensable(const std::vector<const matter::Substance *> &candidates,
		double temperature, double pressure)
{
	const matter::Substance *best = nullptr;
	double share = 0;
	for (const matter::Substance *s : candidates) {
		if (temperature >= s->critical.t)
			continue;
		double e = temperature < s->triple.t ? s->sublimationPressure(temperature) : s->vapourPressure(temperature);
		double x = e / pressure;
		if (x < 1 && x > share) {
			share = x;
			best = s;
		}
	}
	if (!best)
		throw FluidError("fluid layer: nothing here can condense");
	return *best;
}

} // namespace fluid
