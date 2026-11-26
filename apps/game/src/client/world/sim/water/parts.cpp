// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/world/sim/water/parts.h"
#include "src/client/render/map/clientmap.h"
#include "src/client/views/game/water/liquidlook.h"
#include "library/matter/fill/fill.h"
#include "library/geometry/shape/shape.h"
#include "src/shared/game/content/nodedef.h"
#include "src/shared/map/light/light.h"
#include "src/shared/map/block/mapblock.h"
#include <CMeshBuffer.h>
#include <algorithm>
#include <chrono>

namespace liquids {

namespace {
const u8 ALPHA = u8(look::ALPHA * 255 + 0.5f);
const video::SColor SKY(255, u32(look::SKY[0] * 255 + 0.5f), u32(look::SKY[1] * 255 + 0.5f), u32(look::SKY[2] * 255 + 0.5f));
} // namespace

s64 micros()
{
	return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

float light::shade(ClientMap &map, const NodeDefManager &ndef, v3s16 p, u32 daynight)
{
	u8 lit = 0;
	const v3s16 up = p + v3s16(0, 1, 0);
	for (v3s16 q : { p, up, up + v3s16(0, 1, 0), up + SIDES[0], up + SIDES[1], up + SIDES[2], up + SIDES[3] }) {
		const MapNode n = map.getNode(q);
		if (n.getContent() != CONTENT_IGNORE)
			lit = std::max(lit, decode_light(n.getLightBlend(daynight, ndef.getLightingFlags(n))));
	}
	return 0.25f + 0.75f * lit / 255.0f;
}

video::SColor light::tint(u8 r, u8 g, u8 b, float shade, float by, bool top, u8 alpha)
{
	const float k = top ? look::SHEEN : 0;
	auto mix = [&](u8 own, u32 sky) { return u32(std::clamp((own * (1 - k) + sky * k) * shade * by, 0.0f, 255.0f)); };
	return video::SColor(alpha, mix(r, SKY.getRed()), mix(g, SKY.getGreen()), mix(b, SKY.getBlue()));
}

video::SColor light::tint(u8 r, u8 g, u8 b, float shade, float by, bool top)
{
	return tint(r, g, b, shade, by, top, ALPHA);
}

void quads::into(scene::SMeshBuffer *mesh, v3f a, v3f c1, v3f c2, v3f d, v3f normal, video::SColor colour)
{
	auto &vertices = mesh->Vertices->Data;
	auto &indices = mesh->Indices->Data;
	const u32 at = vertices.size();
	if (at + 4 > 65535)
		return; // past what one buffer draws (a block's water never is)
	for (const v3f &p : { a, c1, c2, d })
		vertices.emplace_back(p * BS, normal, colour, v2f(0, 0));
	for (u16 i : { 0, 1, 2, 2, 3, 0 })
		indices.push_back(u16(at + i));
}

scene::SMeshBuffer *quads::emptied(scene::SMeshBuffer *&mesh)
{
	if (!mesh) {
		mesh = new scene::SMeshBuffer();
		mesh->setHardwareMappingHint(scene::EHM_STREAM);
	}
	mesh->Vertices->Data.clear();
	mesh->Indices->Data.clear();
	return mesh;
}

float besideLevel(ClientMap &map, const NodeDefManager &ndef, v3s16 p)
{
	const MapNode n = map.getNode(p);
	if (n.getContent() == CONTENT_IGNORE)
		return 2;
	const ContentFeatures &f = ndef.get(n);
	if (f.drawtype == NDT_LIQUID)
		return (n.getParam2() & fill::HELD) ? fill::partOf(n.getParam2()) / 16.0f : 1; // full, or a river's top
	const std::optional<shape::Shape> s = map.solidShape(p);
	if (!s && f.walkable && f.param_type_2 != CPT2_LEVELED)
		return 2;
	const fill::Fill *there = map.getFill(p);
	if (!there)
		return -1;
	return fill::Room::of(s ? &*s : nullptr).surface(there->amount / 255.0f);
}

std::array<float, 4> held::corners(const Around &around)
{
	const s32 own = around[1][1];
	std::array<float, 4> out;
	for (int k = 0; k < 4; k++) {
		const int ox = k & 1, oz = k >> 1; // the corner's nodes: [oz, oz + 1] × [ox, ox + 1] of `around`
		std::array<s32, 4> levels;
		size_t n = 0;
		for (int dz = 0; dz < 2; dz++)
			for (int dx = 0; dx < 2; dx++)
				if (const s32 l = around[oz + dz][ox + dx]; l != NONE)
					levels[n++] = l;
		std::sort(levels.begin(), levels.begin() + n);
		// the clusters, from the lowest up: a new one where a level is more than
		// a block over its cluster's lowest; the corner is own's cluster's mean
		s64 sum = 0;
		int count = 0;
		for (size_t i = 0, start = 0; i < n; i++) {
			if (levels[i] - levels[start] > 16) {
				if (own >= levels[start] && own < levels[i])
					break;
				start = i;
				sum = count = 0;
			}
			sum += levels[i];
			count++;
		}
		out[k] = float(sum) / count;
	}
	return out;
}

float held::at(const std::array<float, 4> &c, float fx, float fz)
{
	return (c[0] * (1 - fx) + c[1] * fx) * (1 - fz) + (c[2] * (1 - fx) + c[3] * fx) * fz;
}

s32 held::levelAt(ClientMap &map, const NodeDefManager &ndef, const Rivers &rivers, v3s16 p)
{
	auto isHeld = [&](v3s16 q) {
		const MapNode n = map.getNode(q);
		return n.getContent() != CONTENT_IGNORE && ndef.get(n).drawtype == NDT_LIQUID && (n.getParam2() & fill::HELD);
	};
	for (s16 dy : { 1, 0, -1 }) {
		const v3s16 q = p + v3s16(0, dy, 0);
		if (!isHeld(q) || isHeld(q + v3s16(0, 1, 0)))
			continue;
		return q.Y * 16 + fill::surface::of(map.getNode(q).getParam2(), riseAt(rivers, q));
	}
	return NONE;
}

} // namespace liquids
