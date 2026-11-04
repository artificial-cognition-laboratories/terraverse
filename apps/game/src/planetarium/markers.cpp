// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "planetarium/markers.h"
#include "constants.h"
#include "irrlichttypes_bloated.h"
#include <IVideoDriver.h>
#include <SMaterial.h>
#include <algorithm>
#include <cmath>

namespace planetarium {

static constexpr int RING = 24; // pieces of a marker's ring

void MarkerLayer::draw(video::IVideoDriver *driver, const Vec &eye, double altitude)
{
	if (m_markers.empty())
		return;
	// tall enough to stand clear of mountains, and to see from orbit; never
	// more than a share of the body (from far out it would dwarf it)
	const double mast = std::min(std::max(40.0, altitude * 0.08), m_sea * 0.12), ring = mast * 0.25;
	m_vertices.clear();
	m_indices.clear();
	const auto put = [&](const Vec &p, video::SColor c) {
		video::S3DVertex v;
		v.Pos = v3f((float)(p.x - eye.x), (float)(p.y - eye.y), (float)(p.z - eye.z)) * BS;
		v.Color = c;
		m_indices.push_back((u16)m_vertices.size());
		m_vertices.push_back(v);
	};
	for (const Marker &m : m_markers) {
		const Vec up{ std::cos(m.latitude) * std::cos(m.longitude), std::sin(m.latitude),
			std::cos(m.latitude) * std::sin(m.longitude) };
		// behind the body: left out (drawn over the land, it would show through)
		const auto seen = [&](double h) {
			const Vec p{ up.x * (m_sea + h), up.y * (m_sea + h), up.z * (m_sea + h) };
			const Vec d{ p.x - eye.x, p.y - eye.y, p.z - eye.z };
			const double along = std::clamp(-(eye.x * d.x + eye.y * d.y + eye.z * d.z) / (d.x * d.x + d.y * d.y + d.z * d.z), 0.0, 1.0);
			return std::hypot(eye.x + d.x * along, eye.y + d.y * along, eye.z + d.z * along) >= m_sea * (1 - 1e-9);
		};
		const double high = std::max(m.altitude, 0.0);
		if (!seen(high))
			continue;
		const Vec east{ -std::sin(m.longitude), 0, std::cos(m.longitude) };
		const Vec north{ -std::sin(m.latitude) * std::cos(m.longitude), std::cos(m.latitude),
			-std::sin(m.latitude) * std::sin(m.longitude) };
		const auto at = [&](double h, double e, double n) {
			const double r = m_sea + h;
			return Vec{ up.x * r + east.x * e + north.x * n, up.y * r + east.y * e + north.y * n,
				up.z * r + east.z * e + north.z * n };
		};
		put(at(seen(0) ? 0 : high, 0, 0), m.colour);
		put(at(high + mast, 0, 0), m.colour);
		for (int i = 0; i < RING; i++) {
			const double a = 2 * M_PI * i / RING, b = 2 * M_PI * (i + 1) / RING;
			put(at(high + 1, ring * std::cos(a), ring * std::sin(a)), m.colour);
			put(at(high + 1, ring * std::cos(b), ring * std::sin(b)), m.colour);
		}
	}
	video::SMaterial mat;
	mat.MaterialType = video::EMT_SOLID;
	mat.FogEnable = false;
	mat.BackfaceCulling = false;
	mat.Thickness = 3;
	mat.ZBuffer = video::ECFN_ALWAYS; // over the land on its own side (the far side's left out above)
	driver->setMaterial(mat);
	driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);
	driver->drawVertexPrimitiveList(m_vertices.data(), (u32)m_vertices.size(), m_indices.data(),
			(u32)m_indices.size() / 2, video::EVT_STANDARD, scene::EPT_LINES, video::EIT_16BIT);
}

} // namespace planetarium
