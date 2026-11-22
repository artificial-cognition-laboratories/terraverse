// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/world/sim/water/types.h"
#include "irr_ptr.h"
#include <SMaterial.h>
#include <memory>
#include <vector>
#include <CMeshBuffer.h>

class ClientMap;
class IShaderSource;
class IShaderUniformSetterRC;
class NodeDefManager;
namespace scene { class ISceneManager; }
namespace video { class ITexture; class IVideoDriver; }

namespace liquids {

class Grid;

/*
	Draws the grid on the GPU (GL 3): each column's top and ground in a float
	texture, raised by liquid_shader over a tile's shape of columns; a level
	tile as one quad. Each top sits on a sixteenth and moves only when its
	water's well past it, so still water is still on screen.
*/
class Draw
{
public:
	struct Opts
	{
		scene::ISceneManager *smgr;
		IShaderSource *shaders;
		const NodeDefManager &ndef;
		Grid &grid;
		const video::SMaterial &material; // the flat layer's, which this one's is made from
	};
	explicit Draw(Opts o);
	~Draw();
	Draw(const Draw &) = delete;
	Draw &operator=(const Draw &) = delete;

	// The grid moved: its texels are written again as its tiles are drawn
	void moved();
	// Its tiles' texels as their tops move, and uploaded. `relight`: the day's light changed
	void texels(ClientMap &map, u32 daynight, bool relight);
	void render(video::IVideoDriver *driver, v3s16 offset);

private:
	bool settle(int t);
	void surfaceTexels(int t);
	void tileTexels(int t, ClientMap &map, u32 daynight);

	scene::ISceneManager *m_smgr;
	const NodeDefManager &m_ndef;
	Grid &m_grid;
	video::ITexture *m_heights = nullptr;
	std::vector<float> m_texels;
	bool m_upload = true;
	scene::SMeshBuffer *m_shape = nullptr; // a tile's columns: a top and four sides each
	scene::SMeshBuffer *m_whole = nullptr; // a level tile: one top
	video::SMaterial m_material;
	std::shared_ptr<struct WaterDraw> m_draw;
	irr_ptr<IShaderUniformSetterRC> m_uniforms;
};

} // namespace liquids
