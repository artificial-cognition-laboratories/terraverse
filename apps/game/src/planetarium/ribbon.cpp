// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "planetarium/ribbon.h"
#include <IVideoDriver.h>
#include <SMaterial.h>
#include <stdexcept>

namespace planetarium {

// Across a ribbon: its edges fade out from this share of its half width.
static constexpr float SOLID = 0.45f;

void Ribbons::add(const std::vector<v3f> &points, const std::vector<video::SColor> &colours, bool closed)
{
	if (points.size() != colours.size())
		throw std::logic_error("planetarium: a ribbon's points and colours differ in number");
	const size_t n = points.size();
	if (n < 2)
		return;
	const u32 first = (u32)m_vertices.size();
	for (size_t i = 0; i < n; i++) {
		const v3f &p = points[i];
		// along the line here, and across it as the camera sees it
		const v3f &before = i > 0 ? points[i - 1] : closed ? points[n - 1] : p;
		const v3f &after = i + 1 < n ? points[i + 1] : closed ? points[0] : p;
		// (in doubles: a far orbit's lengths in floats overflow when squared, leaving a ribbon no width)
		const v3f a = after - before;
		const v3d along(a.X, a.Y, a.Z), out(p.X, p.Y, p.Z), crossed = along.crossProduct(out);
		const double length = crossed.getLength();
		v3f across = length > 0 ? v3f((f32)(crossed.X / length), (f32)(crossed.Y / length), (f32)(crossed.Z / length)) : v3f();
		// as wide on screen however far: a pixel is the distance over focal
		across *= (f32)(p.getLength() / m_focal) * m_half;
		video::SColor edge = colours[i];
		edge.setAlpha(0);
		m_vertices.emplace_back(p - across, v3f(), edge, v2f());
		m_vertices.emplace_back(p - across * SOLID, v3f(), colours[i], v2f());
		m_vertices.emplace_back(p + across * SOLID, v3f(), colours[i], v2f());
		m_vertices.emplace_back(p + across, v3f(), edge, v2f());
	}
	const size_t pieces = closed ? n : n - 1;
	for (size_t i = 0; i < pieces; i++) {
		const u32 a = first + (u32)i * 4, b = first + (u32)((i + 1) % n) * 4;
		for (u32 k = 0; k < 3; k++) {
			// each of the three bands across: two triangles
			m_indices.insert(m_indices.end(), { a + k, b + k, a + k + 1, a + k + 1, b + k, b + k + 1 });
		}
	}
}

void Ribbons::draw(video::IVideoDriver *driver, bool depth) const
{
	if (m_indices.empty())
		return;
	video::SMaterial mat;
	mat.MaterialType = video::EMT_TRANSPARENT_VERTEX_ALPHA;
	mat.BackfaceCulling = false;
	mat.FogEnable = false;
	mat.ZBuffer = depth ? video::ECFN_LESSEQUAL : video::ECFN_DISABLED;
	mat.ZWriteEnable = video::EZW_OFF;
	mat.ColorMask = video::ECP_RGB;
	driver->setMaterial(mat);
	driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);
	driver->drawVertexPrimitiveList(m_vertices.data(), (u32)m_vertices.size(), m_indices.data(), (u32)m_indices.size() / 3,
			video::EVT_STANDARD, scene::EPT_TRIANGLES, video::EIT_32BIT);
}

} // namespace planetarium
