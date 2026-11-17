// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "client/atlas_view.h"
#include "client/content_mapblock.h"
#include "client/mapblock_mesh.h"
#include "client/mesh_generator_thread.h"
#include "client/meshgen/collector.h"
#include "client/node_visuals.h"
#include "constants.h"
#include "dummygamedef.h"
#include "itemdef.h"
#include "mapblock.h"
#include "map.h"
#include "mapsector.h"
#include "nodedef.h"
#include "planet/atlas.h"
#include "util/numeric.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>

/*
	An atlas planet as a client draws it, against the planet itself.

	A world is laid on the atlas (rolling ground that climbs through the
	shell floors), and from each of many eyes round the planet the client's
	own code runs over it: the blocks the server sends (Atlas::blocksAt),
	each block's nodes with its rim over the seam (QueuedMeshUpdate), the
	engine's mesher, and every block's matrix (AtlasView). Then rays from
	the eye meet what was drawn, and the same rays march through the planet
	(Atlas::locate: no storage, no meshes). Where they disagree is a hole, a
	block drawn where there's air, or two faces drawn in one place.

	Every block is meshed once all are in: the client makes a mesh again
	when a block it read over a seam arrives (QueuedMeshUpdate::read).
*/
class TestAtlasRender : public TestBase {
public:
	TestAtlasRender() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestAtlasRender"; }

	void runTests(IGameDef *gamedef);

	void testViewsMatchThePlanet();
	void testWithinBudget();
	void testClippingSpillsOver();
};

static TestAtlasRender g_test_instance;

void TestAtlasRender::runTests(IGameDef *gamedef)
{
	TEST(testWithinBudget);
	TEST(testClippingSpillsOver);
	if (!std::getenv("ATLAS_BUDGET")) // ATLAS_BUDGET: the costs alone (the views take minutes)
		TEST(testViewsMatchThePlanet);
}

namespace {

using V = terragen::Atlas::Vec;

// the world's planet (Terra Firma's default atlas)
const terragen::Atlas::Params PLANET = terragen::Atlas::standard(8192 / (2 * M_PI));

constexpr double RANGE = 48;  // nodes: how far the client draws, here
constexpr double SLACK = 1.5; // nodes along a ray: a seam's own mismatch, and a ray's grazing

// The ground's height over the sea (nodes) at a direction from the centre:
// hills several hundred nodes across, from 24 under to 36 over (through the
// floors of the shells above and below the sea's), smooth everywhere (no
// longitude: the poles are no different).
double ground(const V &p)
{
	const double r = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
	const double x = p.x / r, y = p.y / r, z = p.z / r;
	return 6 + 24 * std::sin(x * 9 + 1) * std::cos(z * 7 - 2) + 6 * std::sin(y * 11 + x * 5);
}

bool solid(const V &p)
{
	const double r = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
	return r < PLANET.radius + ground(p);
}

class GameDef : public DummyGameDef {
public:
	content_t stone, seam;
	GameDef()
	{
		stone = add("stone", NDT_NORMAL, true);
		seam = add("seam", NDT_AIRLIKE, false);
		NodeDefManager *ndef = const_cast<NodeDefManager *>(m_nodedef);
		ndef->resolveCrossrefs();
		ndef->applyFunction([](ContentFeatures &f) {
			if (!f.visuals)
				f.visuals = std::make_unique<NodeVisuals>();
		});
	}

private:
	content_t add(const std::string &name, NodeDrawType drawtype, bool opaque)
	{
		ItemDefinition item;
		item.type = ITEM_NODE;
		item.name = "test:" + name;
		static_cast<IWritableItemDefManager *>(m_itemdef)->registerItem(item);
		ContentFeatures f;
		f.visuals = std::make_unique<NodeVisuals>();
		f.name = item.name;
		f.drawtype = drawtype;
		f.alpha = ALPHAMODE_OPAQUE;
		f.walkable = true;
		if (!opaque) {
			f.param_type = CPT_LIGHT;
			f.light_propagates = true;
			f.sunlight_propagates = true;
		}
		for (TileDef &t : f.tiledef)
			t.name = name + ".png";
		for (TileSpec &t : f.visuals->tiles)
			t.layers[0].texture_id = 1;
		return const_cast<NodeDefManager *>(m_nodedef)->set(f.name, std::move(f));
	}
};

// A client's map: the blocks it was sent, each filled from the world.
class ClientWorld : public Map
{
public:
	ClientWorld(GameDef *gamedef, const planet::Atlas &atlas) : Map(gamedef), m_def(*gamedef), m_atlas(atlas)
	{
		setAtlas(&atlas);
	}
	bool maySaveBlocks() override { return false; }

	void receive(v3s16 bp)
	{
		const v2s16 p2(bp.X, bp.Z);
		MapSector *&sector = m_sectors[p2];
		if (!sector)
			sector = new MapSector(this, p2, m_gamedef);
		if (sector->getBlockNoCreateNoEx(bp.Y))
			return;
		MapBlock *block = sector->createBlankBlock(bp.Y);
		const v3s16 low = bp * MAP_BLOCKSIZE;
		for (s16 z = 0; z < MAP_BLOCKSIZE; z++)
			for (s16 y = 0; y < MAP_BLOCKSIZE; y++)
				for (s16 x = 0; x < MAP_BLOCKSIZE; x++) {
					const planet::Atlas::Node n{ low.X + x, low.Y + y, low.Z + z };
					const auto c = m_atlas.cell(n);
					const content_t id = !c ? m_def.seam : solid(m_atlas.body(planet::Atlas::HOME).centre(*c)) ? m_def.stone : CONTENT_AIR;
					block->setNodeNoCheck(x, y, z, MapNode(id));
				}
		block->expireIsAirCache();
	}

private:
	GameDef &m_def;
	const planet::Atlas &m_atlas;
};

struct Triangle
{
	v3f a, b, c;
	v3s16 block;
};

// Where a ray from the origin first meets a triangle (either side), if it
// does; `edge`, how near its edge (in the triangle's barycentric terms).
std::optional<f32> meet(const v3f &dir, const Triangle &t, f32 *edge = nullptr)
{
	const v3f e1 = t.b - t.a, e2 = t.c - t.a;
	const v3f p = dir.crossProduct(e2);
	const f32 det = e1.dotProduct(p);
	if (std::fabs(det) < 1e-9f)
		return std::nullopt;
	const v3f s = -t.a;
	// (a hair over its edges: a ray down the diagonal two triangles share
	// missed both by a float's rounding, and a whole face read as a hole)
	constexpr f32 HAIR = 1e-5f;
	const f32 u = s.dotProduct(p) / det;
	if (u < -HAIR || u > 1 + HAIR)
		return std::nullopt;
	const v3f q = s.crossProduct(e1);
	const f32 v = dir.dotProduct(q) / det;
	if (v < -HAIR || u + v > 1 + HAIR)
		return std::nullopt;
	const f32 t_ = e2.dotProduct(q) / det;
	if (edge)
		*edge = std::min({ u, v, 1 - u - v });
	return t_ > 0 ? std::optional<f32>(t_) : std::nullopt;
}

struct Eye
{
	double lat, lon, above; // degrees, nodes over the ground
};

struct Faults
{
	int rays = 0, holes = 0, phantoms = 0, doubled = 0, untransformed = 0;
	int total() const { return holes + phantoms + doubled + untransformed; }
	// what it cost: blocks sent, choosing them (ms), making their meshes (ms)
	size_t blocks = 0;
	double choosing = 0, meshing = 0, preparing = 0; // preparing: the atlas's geometry, off the main thread
};

Faults look(const planet::Atlas &atlas, GameDef &def, const Eye &e, bool rays = true)
{
	const terragen::Atlas &geometry = atlas.body(planet::Atlas::HOME);
	Faults f;
	const double lat = e.lat * M_PI / 180, lon = e.lon * M_PI / 180;
	const V out{ std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon) };
	const double r = PLANET.radius + ground(out) + e.above;
	const V where{ r * out.x, r * out.y, r * out.z };
	const planet::Atlas::Node n = atlas.node(planet::Atlas::Cell(planet::Atlas::HOME, geometry.locate(where)));
	const v3f eye(n.x, n.y, n.z);
	if (std::getenv("ATLAS_EYE")) {
		const auto c = *atlas.cell(n);
		rawstream << "  eye node (" << n.x << "," << n.y << "," << n.z << ") region " << (int)c.region << " "
			<< c.index << " i " << c.i << " j " << c.j << " k " << c.k << " ground " << ground(where) << std::endl;
	}

	using Clock = std::chrono::steady_clock;
	const auto ms = [](Clock::time_point a, Clock::time_point b) {
		return std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count();
	};
	const Clock::time_point started = Clock::now();

	// what the server sends: every slot of the eye's grid in range
	ClientWorld map(&def, atlas);
	const s16 reach = (s16)std::ceil(RANGE / MAP_BLOCKSIZE) + 1;
	const v3s16 centre = getContainerPos(v3s16(n.x, n.y, n.z), MAP_BLOCKSIZE);
	std::set<v3s16> sent;
	for (s16 z = -reach; z <= reach; z++)
		for (s16 y = -reach; y <= reach; y++)
			for (s16 x = -reach; x <= reach; x++) {
				const v3s16 slot = centre + v3s16(x, y, z);
				const double middle = MAP_BLOCKSIZE / 2 - 0.5;
				const double sx = slot.X * MAP_BLOCKSIZE + middle, sy = slot.Y * MAP_BLOCKSIZE + middle,
					sz = slot.Z * MAP_BLOCKSIZE + middle;
				const v3f off(sx - eye.X, sy - eye.Y, sz - eye.Z);
				if (off.getLength() > RANGE + MAP_BLOCKSIZE)
					continue;
				for (const planet::Atlas::Block b : atlas.blocksAt(n, sx, sy, sz))
					sent.insert(v3s16(b.x, b.y, b.z));
			}
	const Clock::time_point chosen = Clock::now();
	for (const v3s16 &b : sent)
		map.receive(b);

	// every block meshed, with what its rim read across a seam
	AtlasView view(atlas);
	const v3s16 offset = floatToInt(eye * BS, BS);
	view.update(eye * BS, eye * BS, offset);
	std::vector<Triangle> triangles;
	std::vector<std::pair<v3f, f32>> spheres; // per block: its triangles' bound
	std::vector<std::pair<size_t, size_t>> spans;
	std::set<v3s16> drawnBlocks;
	Clock::duration meshing{}, preparing{};
	for (const v3s16 &bp : sent) {
		// the atlas's geometry for it, first: found on a thread of its own in game
		// (MeshUpdateQueue::Preparer), so not the main thread's to pay
		const Clock::time_point prepareFrom = Clock::now();
		atlas.prepare({ bp.X, bp.Y, bp.Z });
		preparing += Clock::now() - prepareFrom;
		const Clock::time_point meshFrom = Clock::now();
		QueuedMeshUpdate q;
		q.p = bp;
		q.retrieveBlocks(&map, 1);
		MeshMakeData data(def.ndef(), MAP_BLOCKSIZE, MeshGrid{ 1 });
		q.fill(&data);
		for (MapBlock *b : q.map_blocks)
			if (b)
				b->refDrop();
		q.map_blocks.clear();

		MeshCollector collector(v3f(0));
		MapblockMeshGenerator(&data, &collector).generate();
		bendOntoAtlas(collector, atlas, bp);
		meshing += Clock::now() - meshFrom;
		size_t count = 0;
		for (const auto &layer : collector.prebuffers)
			for (const PreMeshBuffer &buf : layer)
				count += buf.indices.size();
		if (!count)
			continue;
		const core::matrix4 *m = view.transform(bp);
		if (!m) {
			if (f.untransformed++ < 3)
				rawstream << "  block (" << bp.X << "," << bp.Y << "," << bp.Z << ") has faces but no transform" << std::endl;
			continue;
		}
		const size_t first = triangles.size();
		v3f lo(1e9f), hi(-1e9f);
		for (const auto &layer : collector.prebuffers)
			for (const PreMeshBuffer &buf : layer)
				for (size_t i = 0; i + 2 < buf.indices.size(); i += 3) {
					v3f p[3];
					for (int k = 0; k < 3; k++) {
						m->transformVect(p[k], buf.vertices[buf.indices[i + k]].Pos);
						p[k] = p[k] / BS + intToFloat(offset, 1) - eye;
						lo = componentwise_min(lo, p[k]);
						hi = componentwise_max(hi, p[k]);
					}
					triangles.push_back({ p[0], p[1], p[2], bp });
				}
		drawnBlocks.insert(bp);
		spans.emplace_back(first, triangles.size());
		spheres.emplace_back((lo + hi) / 2, (hi - lo).getLength() / 2);
	}

	const Clock::time_point meshed = Clock::now();
	f.blocks = sent.size();
	f.choosing = ms(started, chosen);
	f.meshing = std::chrono::duration<double, std::milli>(meshing).count();
	f.preparing = std::chrono::duration<double, std::milli>(preparing).count();
	if (!rays)
		return f;

	// the planet along a ray: the eye's grid, flat, as the view draws it
	const auto ef = *atlas.frame(n, { eye.X, eye.Y, eye.Z });
	// the planet at a point of the eye's grid
	const auto solidHere = [&](const v3f &d) {
		const V p{ ef.origin.x + ef.axes[0].x * d.X + ef.axes[1].x * d.Y + ef.axes[2].x * d.Z,
			ef.origin.y + ef.axes[0].y * d.X + ef.axes[1].y * d.Y + ef.axes[2].y * d.Z,
			ef.origin.z + ef.axes[0].z * d.X + ef.axes[1].z * d.Y + ef.axes[2].z * d.Z };
		const double pr = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
		return pr < PLANET.radius + PLANET.height - 1 && solid(geometry.centre(geometry.locate(p)));
	};
	const auto solidAt = [&](const v3f &dir, f32 t) {
		const v3f d = dir * t;
		return solidHere(d);
	};
	const auto truth = [&](const v3f &dir) -> std::optional<f32> {
		for (f32 t = 0.05f; t < RANGE; t += 0.05f)
			if (solidAt(dir, t))
				return t;
		return std::nullopt;
	};

	// the eye's up: away from the planet's centre, in its grid
	const v3f up(0, 1, 0);
	const v3f east(1, 0, 0), north = up.crossProduct(east);
	for (int el = -80; el <= -6; el += 4)
		for (int az = 0; az < 360; az += 5) {
			const f32 ce = std::cos(el * M_PI / 180), se = std::sin(el * M_PI / 180);
			v3f dir = east * (ce * std::cos(az * M_PI / 180)) + north * (ce * std::sin(az * M_PI / 180)) + up * se;
			dir.normalize();
			f.rays++;
			struct Hit
			{
				f32 t, edge;
				v3s16 block;
				size_t tri;
			};
			std::vector<Hit> hits;
			for (size_t s = 0; s < spheres.size(); s++) {
				const v3f c = spheres[s].first;
				const f32 along = c.dotProduct(dir);
				if ((c - dir * along).getLength() > spheres[s].second)
					continue;
				for (size_t i = spans[s].first; i < spans[s].second; i++)
				{
					f32 edge;
					if (const auto t = meet(dir, triangles[i], &edge))
						hits.push_back({ *t, edge, triangles[i].block, i });
				}
			}
			std::sort(hits.begin(), hits.end(), [](const Hit &a, const Hit &b) { return a.t < b.t; });
			// a face where the planet is solid on one side and air on the other
			// (a ray clipping a step's edge meets it where the march, a step at a
			// time, saw only air)
			const auto trueFace = [&](size_t tri, const v3f &at) {
				const Triangle &t = triangles[tri];
				v3f normal = (t.b - t.a).crossProduct(t.c - t.a);
				normal.normalize();
				return solidHere(at + normal * 0.1f) != solidHere(at - normal * 0.1f);
			};
			const std::optional<f32> drawn = hits.empty() || hits[0].t > RANGE ? std::nullopt
				: std::optional<f32>(hits[0].t);
			const std::optional<f32> real = truth(dir);
			const auto say = [&](const char *what) {
				rawstream << "  " << what << " at el " << el << " az " << az << ": drawn ";
				if (drawn)
					rawstream << *drawn << " (block " << hits[0].block.X << "," << hits[0].block.Y << ","
						<< hits[0].block.Z << ")";
				else
					rawstream << "nothing";
				rawstream << ", planet " << (real ? *real : -1.0f) << std::endl;
				if (!real || !std::getenv("ATLAS_EYE"))
					return;
				// the planet's cell there: where it's stored, and whether its block came and was drawn
				const v3f d = dir * (*real + 0.05f);
				const V pt{ ef.origin.x + ef.axes[0].x * d.X + ef.axes[1].x * d.Y + ef.axes[2].x * d.Z,
					ef.origin.y + ef.axes[0].y * d.X + ef.axes[1].y * d.Y + ef.axes[2].y * d.Z,
					ef.origin.z + ef.axes[0].z * d.X + ef.axes[1].z * d.Y + ef.axes[2].z * d.Z };
				const auto c = geometry.locate(pt);
				const auto sn = atlas.node(planet::Atlas::Cell(planet::Atlas::HOME, c));
				const v3s16 b = getContainerPos(v3s16(sn.x, sn.y, sn.z), MAP_BLOCKSIZE);
				rawstream << "    planet's cell: region " << (int)c.region << " " << c.index << " i "
					<< c.i << " j " << c.j << " k " << c.k << "; block (" << b.X << "," << b.Y << "," << b.Z << ")"
					<< (sent.count(b) ? " sent" : " NOT SENT") << (drawnBlocks.count(b) ? ", drawn" : ", not drawn");
				const auto cut = atlas.cut(sn);
				rawstream << (cut ? ", cut by " + std::to_string(cut->planes.size()) : ", whole")
					<< (atlas.interior({ b.X, b.Y, b.Z }) ? ", interior block" : ", seam block") << std::endl;
			};
			// clearly so, not a ray grazing a cell's corner: the planet solid a
			// little past where it starts (a hole), or air a little past the
			// drawn face (a phantom)
			if (real && *real < RANGE - 2 * SLACK && (!drawn || *drawn > *real + SLACK) && solidAt(dir, *real + 0.15f)) {
				if (f.holes++ < 3)
					say("hole");
			} else if (drawn && (!real || *drawn < *real - SLACK) && !solidAt(dir, *drawn + 0.02f) &&
					!solidAt(dir, *drawn + 0.15f) && !trueFace(hits[0].tri, dir * *drawn)) {
				if (f.phantoms++ < 3)
					say("phantom");
			}
			// two blocks' faces at one place on the ray, well inside both:
			// they fight. Not two faces meeting at an edge, nor two over a
			// seam overlapping by the view's own error (a block is drawn flat:
			// 16²/8R, under a tenth of a node at its corners)
			const f32 inner = 0.1f;
			if (hits.size() > 1 && hits[1].t - hits[0].t < 0.01f && !(hits[1].block == hits[0].block) &&
					hits[0].edge > inner && hits[1].edge > inner) {
				if (f.doubled++ < 3) {
					say("doubled");
					rawstream << "    and block (" << hits[1].block.X << "," << hits[1].block.Y << "," << hits[1].block.Z
						<< ") at " << hits[1].t << std::endl;
				}
			}
		}
	return f;
}

} // namespace

void TestAtlasRender::testViewsMatchThePlanet()
{
	const planet::Atlas atlas = planet::Atlas::resident(PLANET);
	GameDef def;
	// band middles, the equator, either side of the bands' seams, the cap's
	// edge, the cap and the pole; low over the ground and high (where the
	// ground below climbs through shell floors)
	std::vector<Eye> eyes;
	for (double lat : { 0.0, 12.0, 33.0, -41.0, 58.0, 71.0, 84.6, 85.4, -86.0, 89.9 })
		for (double lon : { 10.0, 137.0 })
			for (double above : { 3.0, 22.0 })
				eyes.push_back({ lat, lon, above });
	// ATLAS_EYE=lat,lon,above: that eye alone (looking into a fault)
	if (const char *one = std::getenv("ATLAS_EYE")) {
		Eye e{};
		if (std::sscanf(one, "%lf,%lf,%lf", &e.lat, &e.lon, &e.above) == 3)
			eyes = { e };
	}
	int failing = 0;
	for (const Eye &e : eyes) {
		const Faults f = look(atlas, def, e);
		rawstream << "eye lat " << e.lat << " lon " << e.lon << " +" << e.above << ": " << f.rays << " rays, "
			<< f.holes << " holes, " << f.phantoms << " phantoms, " << f.doubled << " doubled, "
			<< f.untransformed << " untransformed" << std::endl;
		failing += f.total() > 0;
	}
	UTEST(failing == 0, "%d of %zu eyes see the planet wrong", failing, eyes.size());
}

/*
	What a view costs, against what a client can afford: a new place in
	view comes in within a couple of seconds, a mesh is quick to make (each
	block's, and again when what it reads over a seam arrives), and a view
	needs about the blocks a flat world's would. From a band, the equator,
	the cap's edge and the pole, the first time each is seen (nothing kept).
*/
void TestAtlasRender::testWithinBudget()
{
	struct Budget
	{
		Eye eye;
		size_t blocks;     // blocks for a view RANGE round
		double choosing;   // ms, the server choosing them, the first time
		double perMesh;    // ms, a block's mesh, on average
	};
	// a flat world's view this far round: about 1600 blocks
	const std::vector<Budget> budgets{
		{ { 12, 10, 3 }, 1000, 1500, 1.0 },
		{ { 0, 137, 22 }, 1000, 1500, 1.0 },
		{ { 89.9, 10, 3 }, 1000, 1500, 1.0 },
		{ { 84.6, 10, 3 }, 1000, 1500, 1.0 },
	};
	GameDef def;
	int over = 0;
	for (const Budget &b : budgets) {
		const planet::Atlas atlas = planet::Atlas::resident(PLANET); // nothing kept from another view
		const Faults f = look(atlas, def, b.eye, false);
		const double perMesh = f.meshing / std::max<size_t>(f.blocks, 1);
		rawstream << "eye lat " << b.eye.lat << " lon " << b.eye.lon << " +" << b.eye.above << ": " << f.blocks
			<< " blocks (budget " << b.blocks << "), choosing " << f.choosing << " ms (" << b.choosing << "), "
			<< perMesh << " ms a mesh (" << b.perMesh << "), " << f.preparing / std::max<size_t>(f.blocks, 1)
			<< " ms preparing it (its own thread)" << std::endl;
		over += f.blocks > b.blocks || f.choosing > b.choosing || perMesh > b.perMesh;
	}
	UTEST(over == 0, "%d of %zu views over budget", over, budgets.size());
}

/*
	A seam block's mesh clipped to its territory (bendOntoAtlas) whose
	buffer is near its 16-bit limit already (fine ground and cut blocks
	fill one): what the clipping adds goes to buffers of its own, each under
	the limit, every index in its buffer. It threw, and the mesh thread died
	with the game.
*/
void TestAtlasRender::testClippingSpillsOver()
{
	const planet::Atlas atlas = planet::Atlas::resident(PLANET);
	const terragen::Atlas &geometry = atlas.body(planet::Atlas::HOME);
	// a block with cut cells: near a band's seam
	std::optional<v3s16> seam;
	for (double lon = 0; lon < 360 && !seam; lon += 7) {
		const double lat = 84.6 * M_PI / 180, lo = lon * M_PI / 180;
		const V out{ std::cos(lat) * std::cos(lo), std::sin(lat), std::cos(lat) * std::sin(lo) };
		const V where{ PLANET.radius * out.x, PLANET.radius * out.y, PLANET.radius * out.z };
		const planet::Atlas::Node n = atlas.node(planet::Atlas::Cell(planet::Atlas::HOME, geometry.locate(where)));
		for (const planet::Atlas::Block b : atlas.blocksAt(n, n.x, n.y, n.z))
			if (!atlas.interior(b)) {
				atlas.prepare(b);
				if (!atlas.cutsIn(b).empty() && atlas.blockChart(b)) {
					seam = v3s16(b.x, b.y, b.z);
					break;
				}
			}
	}
	UASSERT(seam);
	const v3s16 low = *seam * MAP_BLOCKSIZE;

	// one buffer: all but a few hundred vertices taken, then a cube's faces
	// over every cut cell (some cross its planes: clipped, they grow)
	MeshCollector collector(v3f(0));
	PreMeshBuffer buf{ TileLayer() };
	buf.vertices.resize(U16_MAX - 300);
	size_t cubes = 0;
	for (const planet::Atlas::CutCell &c : atlas.cutsIn({ seam->X, seam->Y, seam->Z })) {
		const v3f at = v3f(c.node.x - low.X, c.node.y - low.Y, c.node.z - low.Z) * BS;
		for (int face = 0; face < 6; face++) {
			const int a = face / 2, sign = face % 2 ? 1 : -1;
			const int u = (a + 1) % 3, v = (a + 2) % 3;
			v3f q[4];
			for (int k = 0; k < 4; k++) {
				v3f p;
				p[a] = sign * BS / 2;
				p[u] = (k == 1 || k == 2 ? 1 : -1) * BS / 2;
				p[v] = (k >= 2 ? 1 : -1) * BS / 2;
				q[k] = at + p;
			}
			v3f normal;
			normal[a] = f32(sign);
			const u16 base = u16(buf.vertices.size());
			if (base + 4 > U16_MAX)
				break;
			for (const v3f &p : q)
				buf.vertices.emplace_back(p, normal, video::SColor(0xffffffff), v2f(0));
			buf.indices.insert(buf.indices.end(), { base, u16(base + 1), u16(base + 2), base, u16(base + 2), u16(base + 3) });
		}
		cubes++;
	}
	UASSERT(cubes > 0);
	collector.prebuffers[0].push_back(std::move(buf));

	bendOntoAtlas(collector, atlas, *seam);
	UASSERT(collector.prebuffers[0].size() > 1); // it spilled
	for (const PreMeshBuffer &b : collector.prebuffers[0]) {
		UASSERT(b.vertices.size() <= U16_MAX);
		for (u16 i : b.indices)
			UASSERT(i < b.vertices.size());
	}
}
