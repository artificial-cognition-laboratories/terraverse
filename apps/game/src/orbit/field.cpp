// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "field.h"

#include "planet/atlas.h"
#include <cmath>
#include <stdexcept>

namespace orbit {

// What `at` asks of the air and the pull alone (thrust and drag are the caller's).
static const Craft BARE{ 1, 0 };

Field::Field(const planet::Atlas &atlas, const std::vector<terragen::Planet> &bodies) : m_atlas(&atlas)
{
	if ((int)bodies.size() != atlas.bodies())
		throw std::invalid_argument("orbit::Field: a planet for each of the atlas's bodies");
	for (const terragen::Planet &p : bodies)
		m_flights.push_back(std::make_unique<Flight>(p));
}

Field::Field(const terragen::Planet &flat)
{
	m_flights.push_back(std::make_unique<Flight>(flat));
}

Field::~Field() = default;

Vec Field::Local::out(const v3f &v) const
{
	return axes[0] * v.X + axes[1] * v.Y + axes[2] * v.Z;
}

v3f Field::Local::in(const Vec &v) const
{
	// the axes inverted, by cofactors
	const Vec r0 = axes[1].cross(axes[2]), r1 = axes[2].cross(axes[0]), r2 = axes[0].cross(axes[1]);
	const double det = axes[0].dot(r0);
	return v3f((f32)(r0.dot(v) / det), (f32)(r1.dot(v) / det), (f32)(r2.dot(v) / det));
}

std::optional<Field::Local> Field::local(const v3d &pos) const
{
	if (!m_atlas) {
		// flat: the map is the patch at the top of the planet, a node a metre
		const Flight &f = *m_flights[0];
		return Local{ &f, { pos.X, f.planet().radius + pos.Y, pos.Z }, { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } } };
	}
	// above the sky too: charted from the column under it
	const std::optional<planet::Atlas::Node> n = m_atlas->charted(planet::Atlas::moverNode(pos.X, pos.Y, pos.Z));
	if (!n)
		return std::nullopt;
	const std::optional<planet::Atlas::Frame> f = m_atlas->frame(*n, { pos.X, pos.Y, pos.Z });
	if (!f)
		return std::nullopt;
	Local l{ m_flights.at(f->body).get(), { f->origin.x, f->origin.y, f->origin.z }, {} };
	for (int i = 0; i < 3; i++)
		l.axes[i] = { f->axes[i].x, f->axes[i].y, f->axes[i].z };
	return l;
}

std::optional<Field::Here> Field::at(const v3d &pos, const v3f &vel) const
{
	const std::optional<Local> l = local(pos);
	if (!l)
		return std::nullopt;
	const Vec a = l->flight->accel(l->point, l->out(vel), BARE, {});
	const double altitude = l->point.length() - l->flight->planet().radius;
	return Here{ l->in(a), (float)l->flight->density(altitude) };
}

v3f Field::carry(const v3d &from, const v3d &to, const v3f &vel) const
{
	if (!m_atlas)
		return vel; // flat: the axes are the same everywhere
	const std::optional<Local> a = local(from), b = local(to);
	if (!a || !b)
		return vel;
	return b->in(a->out(vel));
}

std::optional<Elements> Field::elements(const v3d &pos, const v3f &vel) const
{
	const std::optional<Local> l = local(pos);
	if (!l)
		return std::nullopt;
	return l->flight->elements({ l->point, l->out(vel) });
}

std::optional<std::vector<Vec>> Field::path(const v3d &pos, const v3f &vel, int points) const
{
	const std::optional<Local> l = local(pos);
	if (!l)
		return std::nullopt;
	return l->flight->path({ l->point, l->out(vel) }, points, 5 * l->flight->planet().radius);
}

} // namespace orbit
