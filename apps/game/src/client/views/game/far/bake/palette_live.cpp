// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/game/far/bake/palette.h"
#include "src/client/render/textures/texturesource.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/game/content/nodedef.h"
#include "src/shared/game/items/itemgroup.h"
#include "src/client/views/game/water/liquidlook.h"
#include <algorithm>

namespace far {

static video::SColorf toColorf(video::SColor c)
{
	return video::SColorf(c.getRed() / 255.0f, c.getGreen() / 255.0f, c.getBlue() / 255.0f, 1.0f);
}

static Palette::Look asDrawn(Palette::Look l, video::SColor colour);

Palette Palette::live(const NodeDefManager *ndef, ITextureSource *tsrc, const std::vector<content_t> &blocks)
{
	Palette out;
	for (content_t id : blocks) {
		if (id == CONTENT_IGNORE || id == CONTENT_AIR || out.m_looks.count(id))
			continue;
		const ContentFeatures &f = ndef->get(id);
		Look l{};
		for (int k = 0; k < 6; k++) {
			const TileDef &t = f.tiledef[k];
			if (t.name.empty())
				throw BaseException("far palette: block '" + f.name + "' has a face with no texture");
			const std::string &overlay = f.tiledef_overlay[k].name;
			const std::string name = overlay.empty() ? t.name : t.name + "^(" + overlay + ")";
			l.face[k] = toColorf(tsrc->getTextureAverageColor(name));
			// a tile's own colour is part of its look; otherwise it takes the node's
			l.tinted[k] = !t.has_color;
			if (t.has_color) {
				const video::SColorf c = toColorf(t.color);
				l.face[k] = video::SColorf(l.face[k].r * c.r, l.face[k].g * c.g, l.face[k].b * c.b, 1);
			}
		}
		// the node's colour: its palette by param2 (as NodeVisuals::getColor), or one
		if (!f.palette_name.empty()) {
			const std::vector<video::SColor> *p = tsrc->getPalette(f.palette_name);
			if (!p)
				throw BaseException("far palette: block '" + f.name + "' has palette '" + f.palette_name +
						"', which won't load");
			for (const video::SColor &c : *p)
				l.tints.push_back(toColorf(c));
		} else {
			l.tints.push_back(toColorf(f.color));
		}
		// the average leaves alpha out: over black and over white, what shows through is the difference
		const std::string &top = f.tiledef[0].name;
		const video::SColorf on_black = toColorf(tsrc->getTextureAverageColor("[fill:1x1:#000000^(" + top + ")"));
		const video::SColorf on_white = toColorf(tsrc->getTextureAverageColor("[fill:1x1:#ffffff^(" + top + ")"));
		l.clarity = std::clamp(((on_white.r - on_black.r) + (on_white.g - on_black.g) + (on_white.b - on_black.b)) / 3.0f,
				0.0f, 1.0f);
		l.over_black = on_black;
		// a liquid the near layer draws is its colour, not its texture's: seen
		// as that layer's tops are, a node deep (the far view's least)
		if (itemgroup_get(f.groups, "liquid_layer"))
			l = asDrawn(std::move(l), f.color);
		out.add(id, std::move(l));
	}
	return out;
}

// The look the liquid layer gives a liquid (liquids::look::top): its sides
// its own colour, its top that over what's under it
static Palette::Look asDrawn(Palette::Look l, video::SColor colour)
{
	const video::SColorf own = toColorf(colour);
	const liquids::look::Top t = liquids::look::top({ own.r, own.g, own.b }, 1);
	for (int k = 0; k < 6; k++) {
		l.face[k] = own;
		l.tinted[k] = false;
	}
	l.over_black = video::SColorf(t.colour[0] * t.alpha, t.colour[1] * t.alpha, t.colour[2] * t.alpha, 1);
	l.clarity = 1 - t.alpha;
	return l;
}

} // namespace far
