// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/figure/figure_mesh.h"
#include <IVideoDriver.h>
#include <SMaterial.h>

namespace figure_mesh {

namespace {

// A box's faces: which way each looks, its two half-axes, and how lit (from above, a little from the front)
struct Face
{
	v3f normal, u, v;
	float light;
};
constexpr Face FACES[] = {
	{ { 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, 1.0f }, { { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, -1 }, 0.45f },
	{ { 1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 }, 0.72f }, { { -1, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 }, 0.86f },
	{ { 0, 0, 1 }, { -1, 0, 0 }, { 0, 1, 0 }, 0.6f }, { { 0, 0, -1 }, { 1, 0, 0 }, { 0, 1, 0 }, 0.8f },
};

video::SColor shaded(physics::BoneColour colour, float light);

} // namespace

Mesh build(const physics::Bones &bones, const core::matrix4 &root)
{
	Mesh out;
	physics::eachBoneBox(bones, [&](const core::matrix4 &bone, v3f size, v3f centre, physics::BoneColour colour) {
		const core::matrix4 m = root * bone;
		const v3f h = size * 0.5f;
		for (const Face &f : FACES) {
			const v3f mid = centre + v3f(f.normal.X * h.X, f.normal.Y * h.Y, f.normal.Z * h.Z);
			const v3f u(f.u.X * h.X, f.u.Y * h.Y, f.u.Z * h.Z), v(f.v.X * h.X, f.v.Y * h.Y, f.v.Z * h.Z);
			const video::SColor c = shaded(colour, f.light);
			const u16 first = (u16)out.vertices.size();
			for (const v3f corner : { mid - u - v, mid + u - v, mid + u + v, mid - u + v }) {
				v3f at = corner;
				m.transformVect(at);
				out.vertices.emplace_back(at, f.normal, c, v2f());
			}
			for (u16 k : { 0, 2, 1, 0, 3, 2 })
				out.indices.push_back(first + k);
		}
	});
	return out;
}

void draw(video::IVideoDriver *driver, const Mesh &mesh)
{
	video::SMaterial mat;
	mat.MaterialType = video::EMT_SOLID;
	mat.FogEnable = false;
	mat.ColorMask = video::ECP_RGB;
	driver->setMaterial(mat);
	driver->drawVertexPrimitiveList(mesh.vertices.data(), (u32)mesh.vertices.size(), mesh.indices.data(),
			(u32)mesh.indices.size() / 3, video::EVT_STANDARD, scene::EPT_TRIANGLES, video::EIT_16BIT);
}

namespace {

video::SColor shaded(physics::BoneColour colour, float light)
{
	const physics::Dye d = physics::dye(colour);
	return video::SColor(255, (u32)(d.r * light), (u32)(d.g * light), (u32)(d.b * light));
}

} // namespace

} // namespace figure_mesh
