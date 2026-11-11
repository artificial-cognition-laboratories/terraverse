// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "grains.h"
#include <algorithm>
#include <cmath>

namespace matter {

const char *const BAND_NAMES[BANDS] = { "stone", "gravel", "sand", "silt", "clay" };
const double BAND_FROM[BANDS] = { 64, 2, 0.05, 0.002, 0 };

Band bandOf(double mm)
{
	for (int b = 0; b < BANDS; b++)
		if (mm >= BAND_FROM[b])
			return (Band)b;
	return Band::Clay;
}

Grains::Grains(const Matter &table) :
	m_table(&table), m_kg(table.substances().size() * BANDS, 0.0), m_organic(table.substances().size())
{
	for (size_t s = 0; s < table.substances().size(); s++) {
		const auto &c = table.substances()[s].composition;
		m_organic[s] = c.count("C") && c.count("H");
	}
}

Grains Grains::broken(const Matter &table, const RockKind &rock, double kg, Band band)
{
	Grains g(table);
	for (const RockKind::Part &p : rock.minerals)
		g.add(p.substance, band, kg * p.share);
	return g;
}

void Grains::add(size_t substance, Band band, double kg)
{
	if (kg < 0 || !std::isfinite(kg))
		throw MatterError("grains: can't add " + std::to_string(kg) + " kg");
	m_kg[at(substance, band)] += kg;
}

void Grains::add(const Grains &other)
{
	if (other.m_table != m_table)
		throw MatterError("grains: mixing grains of two matter tables");
	for (size_t i = 0; i < m_kg.size(); i++)
		m_kg[i] += other.m_kg[i];
	water += other.water;
}

double Grains::mass() const
{
	double sum = 0;
	for (double kg : m_kg)
		sum += kg;
	return sum;
}

double Grains::mass(Band band) const
{
	double sum = 0;
	for (size_t s = 0; s < m_organic.size(); s++)
		sum += m_kg[at(s, band)];
	return sum;
}

double Grains::organic() const
{
	double sum = 0;
	for (size_t s = 0; s < m_organic.size(); s++)
		if (isOrganic(s))
			for (int b = 0; b < BANDS; b++)
				sum += m_kg[at(s, (Band)b)];
	const double all = mass();
	return all > 0 ? sum / all : 0;
}

double Grains::stones() const
{
	const double all = mass();
	return all > 0 ? (mass(Band::Stone) + mass(Band::Gravel)) / all : 0;
}

Texture Grains::texture() const
{
	double by[BANDS] = {};
	for (size_t s = 0; s < m_organic.size(); s++)
		if (!isOrganic(s))
			for (int b = (int)Band::Sand; b < BANDS; b++)
				by[b] += m_kg[at(s, (Band)b)];
	const double fine = by[(int)Band::Sand] + by[(int)Band::Silt] + by[(int)Band::Clay];
	if (fine <= 0)
		return { 0, 0, 0 };
	return { by[(int)Band::Sand] / fine, by[(int)Band::Silt] / fine, by[(int)Band::Clay] / fine };
}

Grains Grains::finer(Band band)
{
	Grains out(*m_table);
	const double all = mass();
	for (size_t s = 0; s < m_organic.size(); s++)
		for (int b = (int)band; b < BANDS; b++)
			std::swap(out.m_kg[at(s, (Band)b)], m_kg[at(s, (Band)b)]);
	const double moved = all > 0 ? out.mass() / all * water : 0;
	out.water = moved;
	water -= moved;
	return out;
}

Grains Grains::share(double fraction)
{
	if (!(fraction >= 0 && fraction <= 1))
		throw MatterError("grains: a share is 0..1, not " + std::to_string(fraction));
	Grains out(*m_table);
	for (size_t i = 0; i < m_kg.size(); i++) {
		out.m_kg[i] = m_kg[i] * fraction;
		m_kg[i] -= out.m_kg[i];
	}
	out.water = water * fraction;
	water -= out.water;
	return out;
}

void Grains::crush(Band band)
{
	for (size_t s = 0; s < m_organic.size(); s++)
		for (int b = 0; b < (int)band; b++) {
			m_kg[at(s, band)] += m_kg[at(s, (Band)b)];
			m_kg[at(s, (Band)b)] = 0;
		}
}

double Grains::weather(double extent, Band crystals, double crumbles)
{
	if (!(extent >= 0))
		throw MatterError("grains: weathering runs forward, not " + std::to_string(extent));
	const std::vector<Substance> &subs = m_table->substances();
	double dissolved = 0;
	// the minerals change first, size by size: what's left is clay-sized
	for (size_t s = 0; s < subs.size(); s++) {
		if (!subs[s].weathers)
			continue;
		const Substance::Weathers &w = *subs[s].weathers;
		const double gone = 1 - std::exp(-extent * w.rate);
		for (int b = 0; b < BANDS; b++) {
			double &kg = m_kg[at(s, (Band)b)];
			const double lost = kg * gone;
			kg -= lost;
			const double left = w.into ? lost * w.keep : 0;
			if (w.into)
				m_kg[at(*w.into, Band::Clay)] += left;
			dissolved += lost - left;
		}
	}
	// then what's coarser than the rock's crystals falls apart into them
	const double apart = 1 - std::exp(-extent * crumbles);
	for (size_t s = 0; s < subs.size(); s++) {
		const Band to = subs[s].grain ? std::max(crystals, bandOf(*subs[s].grain)) : crystals;
		for (int b = 0; b < (int)to; b++) {
			const double moved = m_kg[at(s, (Band)b)] * apart;
			m_kg[at(s, (Band)b)] -= moved;
			m_kg[at(s, to)] += moved;
		}
	}
	return dissolved;
}

} // namespace matter
