// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "atlas_view.h"
#include "constants.h" // BS
#include "util/numeric.h"
#include <algorithm>
#include <cmath>

namespace {

using Plane = planet::Atlas::Cut::Plane;

// A polygon less what a plane cuts off (Sutherland–Hodgman)
std::vector<v3f> keep(const std::vector<v3f> &poly, const Plane &plane)
{
	std::vector<v3f> out;
	for (size_t i = 0; i < poly.size(); i++) {
		const v3f &a = poly[i], &b = poly[(i + 1) % poly.size()];
		const double fa = plane.at(a.X, a.Y, a.Z), fb = plane.at(b.X, b.Y, b.Z);
		if (fa >= 0)
			out.push_back(a);
		if ((fa >= 0) != (fb >= 0))
			out.push_back(a + (b - a) * (f32)(fa / (fa - fb)));
	}
	return out;
}

// Where a plane crosses a box: its polygon, in order round it
std::vector<v3f> section(const aabb3f &box, const Plane &plane)
{
	const f32 lo[3] = { box.MinEdge.X, box.MinEdge.Y, box.MinEdge.Z }, hi[3] = { box.MaxEdge.X, box.MaxEdge.Y, box.MaxEdge.Z };
	std::vector<v3f> points;
	for (int axis = 0; axis < 3; axis++) {
		if (std::abs(plane.normal[axis]) < 1e-12)
			continue;
		const int b = (axis + 1) % 3, c = (axis + 2) % 3;
		for (int e = 0; e < 4; e++) {
			double d[3];
			d[b] = e & 1 ? hi[b] : lo[b];
			d[c] = e & 2 ? hi[c] : lo[c];
			d[axis] = -(plane.normal[b] * d[b] + plane.normal[c] * d[c] + plane.offset) / plane.normal[axis];
			if (d[axis] >= lo[axis] && d[axis] <= hi[axis])
				points.emplace_back(d[0], d[1], d[2]);
		}
	}
	if (points.size() < 3)
		return {};
	v3f middle;
	for (const v3f &p : points)
		middle += p;
	middle /= (f32)points.size();
	const v3f n(plane.normal[0], plane.normal[1], plane.normal[2]);
	const v3f e1 = (std::abs(n.X) < 0.9f ? v3f(1, 0, 0) : v3f(0, 1, 0)).crossProduct(n).normalize();
	const v3f e2 = n.crossProduct(e1);
	std::sort(points.begin(), points.end(), [&](const v3f &p, const v3f &q) {
		return std::atan2((p - middle).dotProduct(e2), (p - middle).dotProduct(e1)) <
				std::atan2((q - middle).dotProduct(e2), (q - middle).dotProduct(e1));
	});
	return points;
}

} // namespace

void AtlasView::update(v3f camera, v3f feet, v3s16 offset)
{
	if (camera == m_camera && feet == m_feet && offset == m_offset)
		return;
	m_camera = camera;
	m_feet = feet;
	m_offset = offset;
	m_frame.clear();
	// the camera's node, or (the camera in a gap between shells, or a seam)
	// the feet's: the player's grid carries on to the camera
	const v3f at = camera / BS, foot = feet / BS;
	const planet::Atlas::Node eye = planet::Atlas::moverNode(at.X, at.Y, at.Z);
	const planet::Atlas::Node stand = planet::Atlas::moverNode(foot.X, foot.Y, foot.Z);
	// (above the atlas's top, charted from the column under it, as movement is: in orbit there's no cell)
	std::optional<planet::Atlas::Node> grid = m_atlas.charted(eye);
	if (!grid)
		grid = m_atlas.charted(stand);
	m_eye = grid ? m_atlas.frame(*grid, { at.X, at.Y, at.Z }) : std::nullopt;
	if (!m_eye)
		return;
	// the eye's axes inverted (rows of the inverse, by cofactors)
	using V = terragen::Atlas::Vec;
	const auto cross = [](V a, V b) { return V{ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; };
	const std::array<V, 3> &j = m_eye->axes;
	const V r0 = cross(j[1], j[2]), r1 = cross(j[2], j[0]), r2 = cross(j[0], j[1]);
	const double det = j[0].x * r0.x + j[0].y * r0.y + j[0].z * r0.z;
	m_inverse = { V{ r0.x / det, r0.y / det, r0.z / det }, V{ r1.x / det, r1.y / det, r1.z / det },
		V{ r2.x / det, r2.y / det, r2.z / det } };
}

const std::optional<planet::Atlas::BlockChart> &AtlasView::chart(v3s16 block)
{
	const auto known = m_blocks.find(block);
	if (known != m_blocks.end())
		return known->second;
	return m_blocks.emplace(block, m_atlas.blockChart({ block.X, block.Y, block.Z })).first->second;
}

const core::matrix4 *AtlasView::transform(v3s16 block)
{
	auto [it, fresh] = m_frame.try_emplace(block);
	if (fresh && m_eye)
		// a block of another body (its tile in a slot near the eye's) is
		// nowhere in the eye's space: not drawn
		if (const auto &c = chart(block); c && *m_atlas.bodyAt(c->cell) == m_eye->body)
			it->second = place(c->centre);
	return it->second ? &*it->second : nullptr;
}

std::optional<core::matrix4> AtlasView::place(const terragen::Atlas::Vec &centre) const
{
	if (!m_eye)
		return std::nullopt;
	// a vertex v (BS, on the planet from C) is the planet's C + v/BS; seen
	// from the eye, in its grid: eye + inverse·(C + v/BS − eye's point);
	// drawn at BS·(that − offset)
	using V = terragen::Atlas::Vec;
	const V d{ centre.x - m_eye->origin.x, centre.y - m_eye->origin.y, centre.z - m_eye->origin.z };
	core::matrix4 w;
	for (int r = 0; r < 3; r++) {
		const V &row = m_inverse[r];
		w[r] = (f32)row.x;
		w[4 + r] = (f32)row.y;
		w[8 + r] = (f32)row.z;
		const double at = m_eye->at[r] + row.x * d.x + row.y * d.y + row.z * d.z;
		w[12 + r] = (f32)((at - (r == 0 ? m_offset.X : r == 1 ? m_offset.Y : m_offset.Z)) * BS);
	}
	return w;
}

std::optional<std::vector<AtlasView::Line>> AtlasView::outline(v3s16 node, const std::vector<aabb3f> &boxes)
{
	const std::optional<planet::Atlas::Cut> cut = m_atlas.cut({ node.X, node.Y, node.Z });
	const std::vector<Plane> planes = cut ? cut->planes : std::vector<Plane>{};
	std::vector<std::vector<v3f>> faces; // node coordinates, from its centre
	for (const aabb3f &b : boxes) {
		const aabb3f box(b.MinEdge / BS, b.MaxEdge / BS);
		v3f k[8]; // bit 0 x, bit 1 y, bit 2 z: max
		for (int i = 0; i < 8; i++)
			k[i] = v3f(i & 1 ? box.MaxEdge.X : box.MinEdge.X, i & 2 ? box.MaxEdge.Y : box.MinEdge.Y,
					i & 4 ? box.MaxEdge.Z : box.MinEdge.Z);
		for (const auto &f : { std::array<int, 4>{ 0, 2, 6, 4 }, { 1, 3, 7, 5 }, { 0, 1, 5, 4 }, { 2, 3, 7, 6 },
					 { 0, 1, 3, 2 }, { 4, 5, 7, 6 } }) {
			std::vector<v3f> poly{ k[f[0]], k[f[1]], k[f[2]], k[f[3]] };
			for (const Plane &p : planes)
				poly = keep(poly, p);
			if (poly.size() >= 3)
				faces.push_back(poly);
		}
		for (size_t i = 0; i < planes.size(); i++) {
			std::vector<v3f> poly = section(box, planes[i]);
			for (size_t o = 0; o < planes.size() && poly.size() >= 3; o++)
				if (o != i)
					poly = keep(poly, planes[o]);
			if (poly.size() >= 3)
				faces.push_back(poly);
		}
	}
	std::vector<Line> lines;
	for (const std::vector<v3f> &f : faces)
		for (size_t i = 0; i < f.size(); i++)
			lines.emplace_back(f[i], f[(i + 1) % f.size()]);
	return place(node, lines);
}

std::optional<std::vector<AtlasView::Line>> AtlasView::place(v3s16 node, const std::vector<std::pair<v3f, v3f>> &lines)
{
	const v3s16 block = getContainerPos(node, MAP_BLOCKSIZE);
	const auto &c = chart(block);
	const core::matrix4 *m = transform(block);
	if (!c || !m)
		return std::nullopt;
	// each point onto the planet as the mesh's vertices are, then drawn as its block is
	const auto scene = [&](const v3f &p) {
		const terragen::Atlas::Vec q = m_atlas.onPlanet(*c, node.X + p.X, node.Y + p.Y, node.Z + p.Z);
		v3f at((f32)q.x * BS, (f32)q.y * BS, (f32)q.z * BS);
		m->transformVect(at);
		return at;
	};
	std::vector<Line> out;
	for (const auto &[a, b] : lines)
		out.emplace_back(scene(a), scene(b));
	return out;
}
