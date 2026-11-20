// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/planetarium/globe/seams.h"
#include "src/shared/base/constants.h"
#include "src/shared/base/irrlichttypes_bloated.h"
#include "src/shared/base/exceptions.h"
#include <IVideoDriver.h>
#include <SMaterial.h>
#include <cmath>
#include <limits>

namespace planetarium {

static constexpr double LIFT = 2;    // blocks over the sea
static constexpr int ALONG = 96;     // pieces of a meridian, cap to cap
static constexpr int ROUND = 288;    // pieces of a cap circle
static const video::SColor SEGMENT(255, 255, 214, 92), CAP(255, 255, 140, 60);

SeamLayer::SeamLayer(const terragen::Atlas::Params &atlas, double sea)
{
	const double r = sea + LIFT;
	const auto at = [&](double lat, double lon) {
		return Vec{ r * std::cos(lat) * std::cos(lon), r * std::sin(lat), r * std::cos(lat) * std::sin(lon) };
	};
	const double cap = atlas.capLatitude * M_PI / 180;
	for (int s = 0; s < atlas.segments; s++) {
		const double lon = s * 2 * M_PI / atlas.segments; // segment s's west edge
		for (int i = 0; i < ALONG; i++) {
			m_points.push_back(at(-cap + 2 * cap * i / ALONG, lon));
			m_points.push_back(at(-cap + 2 * cap * (i + 1) / ALONG, lon));
		}
	}
	const size_t meridians = m_points.size();
	for (double lat : { cap, -cap })
		for (int i = 0; i < ROUND; i++) {
			m_points.push_back(at(lat, 2 * M_PI * i / ROUND));
			m_points.push_back(at(lat, 2 * M_PI * (i + 1) / ROUND));
		}
	if (m_points.size() > std::numeric_limits<u16>::max())
		throw BaseException("planetarium: too many seam lines for 16-bit indices");
	m_vertices.resize(m_points.size());
	for (size_t i = 0; i < m_points.size(); i++) {
		m_vertices[i].Color = i < meridians ? SEGMENT : CAP;
		m_indices.push_back((u16)i);
	}
}

void SeamLayer::draw(video::IVideoDriver *driver, const Vec &eye)
{
	for (size_t i = 0; i < m_points.size(); i++) {
		const Vec &p = m_points[i];
		m_vertices[i].Pos = v3f((float)(p.x - eye.x), (float)(p.y - eye.y), (float)(p.z - eye.z)) * BS;
	}
	video::SMaterial m;
	m.MaterialType = video::EMT_SOLID;
	m.FogEnable = false;
	m.BackfaceCulling = false;
	m.Thickness = 2;
	driver->setMaterial(m);
	driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);
	driver->drawVertexPrimitiveList(m_vertices.data(), (u32)m_vertices.size(), m_indices.data(),
			(u32)m_indices.size() / 2, video::EVT_STANDARD, scene::EPT_LINES, video::EIT_16BIT);
}

} // namespace planetarium
