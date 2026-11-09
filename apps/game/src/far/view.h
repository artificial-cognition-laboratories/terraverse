// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "far/terrain.h"
#include "irr_ptr.h"
#include "irrlichttypes_bloated.h"
#include <ISceneNode.h>
#include <SMaterial.h>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

class AtlasView;
class Camera;
class ClientMap;
class IShaderSource;
class ITextureSource;
class NodeDefManager;
namespace video { class ITexture; }
namespace planet { class Atlas; }

namespace far {

// The near world's edge this frame (scene units, from the eye; 0: no far view,
// none): the far view draws past it and the near world's blocks only short
// of it (nodes_shader's nearCut), so the two never overlap. Blocks past the
// near range were drawn whole for a few frames (the draw list isn't remade
// every frame, nor cut finer than a block), and the far land's coarser banks
// and cliffs stood up through them.
float nearCut();

class Palette;
class Tiles;
struct DrawState;
class Uniforms;

/*
	The far view: the planet beyond the loaded map blocks, out to the
	horizon, seen from the player's camera. Its tiles are far::Tiles' for
	the player's eye, placed where the near world's blocks are drawn
	(AtlasView), so the two are one surface; up through the air the haze
	gives way to the whole planet.

	It draws only past the near world's range (the cut, from the eye):
	nearer, the near world's blocks are the land; a tile wholly inside it
	isn't drawn at all.

	Drawn right after the sky with the camera's depth projection. Its depth
	remains for near blocks and clouds, so a cloud behind distant terrain is
	occluded.
*/
class View : public scene::ISceneNode
{
public:
	View(scene::ISceneManager *smgr, IShaderSource *ssrc, ITextureSource *tsrc,
			const NodeDefManager *ndef, const Horizon &horizon, const planet::Atlas &atlas, int body);
	~View();

	// Once a frame, before drawing. `near_range`: how far the map blocks are
	// drawn, in blocks. `fog_limit`: a server's fog distance, in blocks.
	void update(const Camera &camera, ClientMap &map, float near_range, bool range_all,
			std::optional<float> fog_limit, v3f velocity);

	// Whether it draws this frame, and how far off what it draws can be
	// (scene units): the fog follows it.
	bool showing() const { return m_showing; }
	float reach() const { return m_reach; }
	const Palette &palette() const { return *m_palette; }

	void OnRegisterSceneNode() override;
	void render() override;
	const aabb3f &getBoundingBox() const override { return m_box; }

	// Rereads the far view's settings (they're watched).
	void readSettings();

private:
	const s16 m_water_level;
	const double m_radius; // blocks
	std::unique_ptr<Palette> m_palette;
	std::unique_ptr<Tiles> m_tiles; // after the palette it reads
	std::shared_ptr<DrawState> m_state;
	irr_ptr<Uniforms> m_uniforms;
	video::SMaterial m_material;
	aabb3f m_box{ -1e7f, -1e7f, -1e7f, 1e7f, 1e7f, 1e7f };

	// settings
	bool m_enabled = true;
	float m_split = 2; // a tile splits while nearer than this many of its spans past the cut
	bool m_debug_levels = false; // far_view_debug_levels: each tile tinted by its level
	float m_max = 0; // past the eye's own horizon, it draws no farther than this

	float m_cut = 0;      // blocks
	float m_reach = 0;    // scene units
	bool m_showing = false;
	float m_altitude = 0; // m above the sea
	float m_share = 0;    // 0 in the air's haze .. 1 above the air
	AtlasView *m_view = nullptr; // where it draws the planet, this frame
	v3f m_eye;                   // where it's drawn from then, in blocks
};

} // namespace far
