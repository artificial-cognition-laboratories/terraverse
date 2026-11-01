// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "far/palette.h"
#include "exceptions.h"
#include <string>

namespace far {

void Palette::add(content_t id, Look look)
{
	if (look.tints.size() != 1 && look.tints.size() != 256)
		throw BaseException("far palette: block " + std::to_string(id) + " has " +
				std::to_string(look.tints.size()) + " tints (one, or 256 by param2)");
	m_looks[id] = std::move(look);
}

const Palette::Look &Palette::look(content_t id) const
{
	auto it = m_looks.find(id);
	if (it == m_looks.end())
		throw BaseException("far palette: no look for block " + std::to_string(id));
	return it->second;
}

video::SColorf Palette::face(MapNode n, Face f) const
{
	const Look &l = look(n.getContent());
	const video::SColorf c = l.face[f];
	if (!l.tinted[f])
		return c;
	const video::SColorf &t = l.tints.size() == 1 ? l.tints[0] : l.tints[n.param2];
	return video::SColorf(c.r * t.r, c.g * t.g, c.b * t.b, 1);
}

video::SColorf Palette::through(MapNode liquid, video::SColorf floor, float lit) const
{
	const Look &l = look(liquid.getContent());
	// colour × opacity, plus what shows through
	return video::SColorf(l.over_black.r + floor.r * lit * l.clarity, l.over_black.g + floor.g * lit * l.clarity,
			l.over_black.b + floor.b * lit * l.clarity, 1);
}

std::vector<content_t> blocksIn(const Content &content)
{
	const terra::Blocks &b = content.blocks;
	std::vector<content_t> out = { b.stone, b.water, b.lava, b.placer, b.soil };
	for (const terra::BiomeBlocks &bb : b.biomes)
		out.insert(out.end(), { bb.top, bb.filler, bb.riverbed, bb.water_top });
	out.insert(out.end(), std::begin(b.rock), std::end(b.rock));
	out.insert(out.end(), std::begin(b.ore), std::end(b.ore));
	for (const terra::Species &s : content.species) {
		out.insert(out.end(), { s.trunk, s.leaves });
		out.insert(out.end(), s.on.begin(), s.on.end());
	}
	return out;
}

} // namespace far
