// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/world/sim/water/liquids.h"
#include "src/client/render/gputime/gputime.h"
#include "library/trace/trace.h"
#include "src/shared/platform/settings/settings.h"
#include "src/shared/platform/system/log.h"
#include "src/shared/platform/system/profiler.h"
#include <ISceneManager.h>
#include <IVideoDriver.h>
#include <algorithm>
#include <cmath>

namespace liquids {

namespace {
video::SMaterial flatMaterial();
bool runsOnGpu(scene::ISceneManager *smgr);
Grid::Opts fineGrid();
} // namespace

Layer::Layer(Opts o) :
	scene::ISceneNode(o.smgr->getRootSceneNode(), o.smgr, -1),
	m_material(flatMaterial()),
	m_gpu(runsOnGpu(o.smgr)),
	m_grid(fineGrid()),
	m_flat({ /*ndef*/ *o.ndef, /*grid*/ m_grid }),
	m_scan({ /*ndef*/ *o.ndef, /*grid*/ m_grid }),
	m_hold({ /*grid*/ m_grid }),
	m_flow({ /*grid*/ m_grid }),
	m_drops({ /*ndef*/ *o.ndef, /*grid*/ m_grid, /*grid_runs*/ m_gpu }),
	m_draw(m_gpu ? std::make_unique<Draw>(Draw::Opts{ /*smgr*/ o.smgr, /*shaders*/ o.shaders, /*ndef*/ *o.ndef,
			/*grid*/ m_grid, /*material*/ m_material }) : nullptr)
{}

void Layer::update(const std::unordered_set<v3s16> &changed, const std::unordered_set<v3s16> &ground,
		const std::vector<Stream> &streams, const std::vector<Mover> &movers, const Rivers &rivers, ClientMap &map,
		v3s16 offset, v3f camera, std::optional<s16> sea, u32 daynight, float dtime, bool hurrying)
{
	m_offset = offset;
	const Frame frame{ offset, camera, sea, &rivers, daynight };
	m_grid.sea_level = sea ? (*sea + 1) * 16 : INT32_MIN;
	const bool relight = m_flat.begin(changed, ground, map, rivers, camera, daynight, dtime);

	std::unordered_set<v3s16> blocks(changed);
	blocks.insert(ground.begin(), ground.end());
	s64 looked = 0, ran = 0;
	// Flown past too fast to see it slosh: the grid waits, and what's new is drawn flat
	const bool grid_runs = m_gpu && !hurrying;
	if (grid_runs) {
		const s64 t0 = micros();
		const Grid::Placed placed = m_grid.place(camera);
		m_flat.letGo(placed.let_go);
		if (placed.moved)
			m_draw->moved();
		const Scan::Handover handover = m_scan.frame(blocks, changed, map, frame, t0);
		m_flat.letGo(handover.let_go);
		m_flat.taken(handover.taken);
		looked = micros() - t0;

		const s64 t1 = micros();
		m_drops.pour(streams);
		m_flow.push(movers, dtime);
		m_hold.frame(dtime);
		const float dt = m_flow.dt();
		m_owed = std::min(m_owed + dtime, dt * m_flow.mostSteps());
		for (; m_owed >= dt; m_owed -= dt) {
			m_flow.step(dt, camera);
			m_hold.carry(dt);
		}
		ran = micros() - t1;
	}
	m_drops.fall(map, dtime);

	const s64 grids = micros();
	g_profiler->graphAdd("Water flat blocks", m_flat.rebuild(map, daynight, hurrying));
	const s64 flat = micros();
	if (grid_runs)
		m_draw->texels(map, daynight, relight);
	m_drops.build(map, offset, daynight);
	const s64 drawn = micros();
	g_profiler->graphAdd("Water look [us]", looked);
	g_profiler->graphAdd("Water run [us]", ran);
	g_profiler->graphAdd("Water flat [us]", flat - grids);
	g_profiler->graphAdd("Water mesh [us]", drawn - flat);
}

void Layer::OnRegisterSceneNode()
{
	if (IsVisible)
		SceneManager->registerNodeForRendering(this, scene::ESNRP_TRANSPARENT);
	ISceneNode::OnRegisterSceneNode();
}

void Layer::render()
{
	TF_ZONE("Water render"); // Terra Firma: inside Draw3D, timed (the debugger)
	gpu::Zone gpu("Water");
	video::IVideoDriver *driver = SceneManager->getVideoDriver();
	if (SceneManager->getSceneNodeRenderPass() != scene::ESNRP_TRANSPARENT)
		return;
	driver->setMaterial(m_material);
	m_flat.render(driver, m_offset);
	if (m_draw) {
		m_draw->render(driver, m_offset);
		driver->setMaterial(m_material);
	}
	m_drops.render(driver);
}

namespace {

video::SMaterial flatMaterial()
{
	video::SMaterial m;
	m.MaterialType = video::EMT_TRANSPARENT_VERTEX_ALPHA;
	m.BackfaceCulling = false;
	m.ZWriteEnable = video::EZW_OFF;
	m.FogEnable = true;
	return m;
}

// The grid's drawn on the GPU where the driver reads a texture in a vertex shader (GL 3)
bool runsOnGpu(scene::ISceneManager *smgr)
{
	if (smgr->getVideoDriver()->getDriverType() != video::EDT_OPENGL3) {
		warningstream << "liquids: water near is run and drawn on OpenGL 3 only; here it's drawn flat" << std::endl;
		return false;
	}
	return g_settings->getBool("water_fine");
}

// A sixteenth, as far as the setting reaches each way (read once: the grid's
// texture is this size for as long as the world's open)
Grid::Opts fineGrid()
{
	const int nodes = int(std::lround(g_settings->getFloat("water_fine_reach") * 2));
	return { /*res*/ 16, /*nodes*/ nodes, /*follow*/ 4, /*scans*/ 24 };
}

} // namespace

} // namespace liquids
