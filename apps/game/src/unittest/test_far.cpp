// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"
#include "exceptions.h"
#include "terragen/worlds.h"

#include <cmath>
#include <map>
#include <set>

#include "far/horizon.h"
#include "far/mesh.h"
#include "far/palette.h"
#include "light.h"
#include "constants.h"
#include <algorithm>
#include "far/tiling.h"
#include "mapgen/atlas_land.h"
#include "far/terrain.h"
#include "mapgen/terra_column.h"
#include "planet/atlas.h"
#include "network/networkpacket.h"
#include "network/networkprotocol.h"
#include "util/pointer.h"

using namespace far;

class TestFar : public TestBase {
public:
	TestFar() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestFar"; }

	void runTests(IGameDef *gamedef);

	void testPacketRoundTrip();
	void testTilingIsTheAtlas();
	void testTileIsTheAtlasLand();
	void testABodysOwnLand();
	void testNoHolesFromAbove();
};

static TestFar g_test_instance;

void TestFar::runTests(IGameDef *gamedef)
{
	TEST(testPacketRoundTrip);
	TEST(testTilingIsTheAtlas);
	TEST(testTileIsTheAtlasLand);
	TEST(testABodysOwnLand);
	TEST(testNoHolesFromAbove);
}

// A horizon with made-up block ids: the far view only compares them.
static Horizon horizon()
{
	Horizon h;
	h.code = "TF19.dGVycmE";
	h.water_level = 1;
	terra::Blocks &b = h.content.blocks;
	content_t next = 100;
	for (terra::BiomeBlocks &bb : b.biomes)
		bb = { next++, next++, next++, 11, 1, 3, 1, 0 };
	b.stone = 10;
	b.water = 11;
	b.lava = 13;
	b.placer = 14;
	b.soil = 15;
	for (content_t &c : b.rock)
		c = next++;
	b.ore[0] = CONTENT_IGNORE;
	for (size_t i = 1; i < std::size(b.ore); i++)
		b.ore[i] = next++;
	terra::Species oak{ "oak", { { -1, 1 }, { -1, 1 }, false, { 0, 0 }, false, false }, {}, 20, 21,
		{ "FFFFA", "[&&fff][++&&fff][----&&fff]", "", "", "", "single", 45, 1, 0, true } };
	for (const terra::BiomeBlocks &bb : b.biomes)
		oak.on.push_back(bb.top);
	h.content.species.push_back(oak);
	return h;
}

void TestFar::testPacketRoundTrip()
{
	Horizon h = horizon();
	h.content.species[0].niche.heat[0] = -0.123456789012345; // exact bits, not a rounding
	NetworkPacket out(TOCLIENT_HORIZON, 0);
	write(out, h);
	// as it travels: the command, then the body
	Buffer<u8> wire = out.oldForgePacket();
	NetworkPacket in;
	in.putRawPacket(*wire, wire.getSize(), 0);
	Horizon r = read(in);
	UASSERT(r.code == h.code && r.water_level == h.water_level);
	UASSERT(r.content.blocks.biomes[3].filler == h.content.blocks.biomes[3].filler);
	UASSERT(r.content.blocks.rock[2] == h.content.blocks.rock[2] && r.content.blocks.soil == h.content.blocks.soil);
	UASSERT(r.content.species.size() == 1 && r.content.species[0].name == "oak");
	UASSERT(r.content.species[0].niche.heat[0] == h.content.species[0].niche.heat[0]);
	UASSERT(r.content.species[0].shape.rules_a == h.content.species[0].shape.rules_a);
	UASSERT(r.content.species[0].on == h.content.species[0].on);
}

// An atlas planet as Terra Firma's worlds are: its far view tiles its regions.
static const terragen::Atlas::Params ATLAS = terragen::Atlas::standard(8192 / (2 * M_PI));

static double apart(const Tiling::Vec &a, const Tiling::Vec &b)
{
	return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
}

// The tiling's roots are the regions with a surface, each covering its whole
// grid; a tile's cell is the atlas cell it stands for, and its point there is
// that cell's centre (the region's own chart).
void TestFar::testTilingIsTheAtlas()
{
	const terragen::Atlas atlas(ATLAS);
	const Tiling t(atlas);
	// every region with a surface, as squares covering its grid; each root's
	// middle is in its region (on its grid's own part of the planet)
	std::set<s32> regions;
	for (const TileKey &root : t.roots()) {
		const terragen::Atlas::Grid &g = atlas.grids().at(root.face);
		UASSERT(g.region != terragen::Atlas::Region::Core);
		UASSERT(t.within(root));
		regions.insert(root.face);
		const Tiling::Vec m = t.middle(root);
		const terragen::Atlas::Cell owner = atlas.owner({ m.x, m.y + 0, m.z });
		UTEST(owner.region == g.region && owner.index == g.index, "root (%d, %d) of grid %d has its middle in region %d/%d",
				root.x, root.z, root.face, (int)owner.region, owner.index);
	}
	UASSERTEQ(size_t, regions.size(), (size_t)ATLAS.segments + 2); // the segments and the caps
	for (const TileKey key : { TileKey{ 0, 3, 40, t.roots()[0].face }, TileKey{ 0, 1, 2, t.roots().back().face } })
		for (s32 j = 0; j < TILE; j += 7)
			for (s32 i = 0; i < TILE; i += 5) {
				const terragen::Atlas::Cell c = t.cell(key, i, j);
				UASSERT(apart(t.point(key, i, 0, j), atlas.centre(c)) < 1e-6);
			}
}

// A level-0 tile is the game's surface, cell for cell: where the atlas has a
// cell, its ground is the column terra::AtlasLand builds there and its top
// block AtlasLand's at that layer; where it has none, nothing is drawn.
void TestFar::testTileIsTheAtlasLand()
{
	Horizon h = horizon();
	const planet::Atlas atlas = planet::Atlas::resident(ATLAS);
	Generator gen(h, atlas, planet::Atlas::HOME);
	{
		// (what a coarse tile costs, for the log: the horizon is hundreds of them)
		const Tiling tiling(atlas.body(planet::Atlas::HOME));
		const TileKey root = tiling.roots()[0];
		const u64 from = porting::getTimeMs();
		const int builds = 8;
		for (int k = 0; k < builds; k++)
			gen.build(TileKey{ 3, k, 2, root.face });
		infostream << "far: a level 3 tile takes " << (porting::getTimeMs() - from) / builds << " ms to build" << std::endl;
	}
	terra::AtlasLand::Reader land(gen.land());
	const Tiling t(atlas.body(planet::Atlas::HOME));
	int cells = 0, gaps = 0;
	// a segment's middle, its east edge (cut cells, the gap past the grid), a cap
	const TileKey seg = t.roots()[0], cap = t.roots().back();
	const terragen::Atlas::Grid &g = atlas.body(planet::Atlas::HOME).grids().at(seg.face);
	const s32 east = (s32)((g.high[0] - g.low[0]) / TILE);
	for (const TileKey key : { TileKey{ 0, 2, 40, seg.face }, TileKey{ 0, east, 40, seg.face }, TileKey{ 0, 3, 3, cap.face } }) {
		const Tile tile = gen.build(key);
		for (s32 j = 0; j < TILE; j++)
			for (s32 i = 0; i < TILE; i++) {
				const Cell &c = tile.at(i, j);
				const terragen::Atlas::Cell cell = t.cell(key, i, j);
				UASSERT(c.space == !atlas.body(planet::Atlas::HOME).exists(cell));
				if (c.space) {
					gaps++;
					continue;
				}
				cells++;
				const planet::Atlas::Node n = atlas.node(planet::Atlas::Cell(planet::Atlas::HOME, cell));
				const terra::Column *column = land.column(n);
				UASSERT(column);
				UASSERTEQ(s32, c.ground, column->height);
				const std::optional<MapNode> top = land.node({ n.x, c.ground, n.z });
				if (top) // the ground's own layer can be trimmed where the surface one isn't
					UASSERT(top->getContent() == c.top.getContent());
			}
	}
	UASSERT(cells > 2000 && gaps > 0);
}

// Another body's far view is built from its own world (terragen/worlds.h),
// and asked for a column of the home's tile, refuses.
void TestFar::testABodysOwnLand()
{
	Horizon h = horizon();
	const terragen::WorldCode home = terragen::WorldCode::decode(h.code);
	const std::vector<terragen::BodyWorld> worlds = terragen::bodyWorlds(home, terragen::Atlas::standard(250000));
	std::vector<terragen::Atlas::Params> params;
	for (const terragen::BodyWorld &w : worlds)
		params.push_back(w.atlas);
	planet::Atlas atlas(params);
	atlas.put(0, planet::Atlas::TileId(planet::Atlas::HOME, 0, 0, 0));
	atlas.put(1, planet::Atlas::TileId(1, 0, 0, 0));
	const Generator moon(h, atlas, 1);
	UASSERT(moon.world().code().text == worlds[1].code.text);
	UASSERT(moon.world().code().size == worlds[1].code.size);
	const s32 x = planet::Atlas::ORIGIN + 100, z = planet::Atlas::ORIGIN + 100; // slot 0: the home's
	EXCEPTION_CHECK(std::logic_error, moon.column(x, z));
	moon.column(x + planet::Atlas::SLOT, z); // slot 1: the moon's
	// a viewer of the home alone (the menu's Planetarium) lays out the home:
	// its far view is the world code's, whatever bodies the system has
	const planet::Atlas homeOnly(terragen::Atlas::standard(250000));
	const Generator own(h, homeOnly, planet::Atlas::HOME);
	UASSERT(own.world().code().text == h.code);
	// and asked for another body it can't hold, it says so
	EXCEPTION_CHECK(BaseException, Generator(h, homeOnly, 1));
}

// Every block one grey: the land and the sea merge into strips wherever
// they can (far/mesh.cpp), the worst case for slits between them.
static Palette greyPalette(const Content &content)
{
	Palette p;
	for (content_t id : blocksIn(content)) {
		Palette::Look l{};
		l.face.fill(video::SColorf(0.5f, 0.5f, 0.5f, 1));
		l.tinted.fill(false);
		l.tints = { video::SColorf(1, 1, 1, 1) };
		l.over_black = video::SColorf(0.5f, 0.5f, 0.5f, 1);
		l.clarity = 0;
		p.add(id, l);
	}
	return p;
}

/*
	The planet from above, every tile `drop` levels under the roots: a ray straight down at
	the centre from anywhere must meet the land's mesh. A miss is a hole the
	background shows through (the planetarium's black lines: region seams,
	slits between rows).
*/
static int holesFromAbove(const terragen::Atlas::Params &params, s32 drop)
{
	Horizon h = horizon();
	const planet::Atlas atlas(params);
	const Generator gen(h, atlas, planet::Atlas::HOME);
	const Tiling t(atlas.body(planet::Atlas::HOME));
	const Palette palette = greyPalette(h.content);
	const double radius = t.radius();

	struct Tri
	{
		Tiling::Vec a, b, c;
	};
	// triangles binned by their direction from the centre, on a grid of BIN
	constexpr double BIN = 0.02;
	std::vector<Tri> tris;
	std::map<std::tuple<int, int, int>, std::vector<size_t>> bins;
	auto binOf = [](double v) { return (int)std::floor(v / BIN); };
	auto unit = [](const Tiling::Vec &v) {
		const double l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
		return Tiling::Vec{ v.x / l, v.y / l, v.z / l };
	};

	for (const TileKey &root : t.roots()) {
		const s32 n = 1 << drop;
		for (s32 z = 0; z < n; z++)
			for (s32 x = 0; x < n; x++) {
				const TileKey key{ root.level - drop, root.x * n + x, root.z * n + z, root.face };
				if (!t.within(key))
					continue;
				Mesh m = mesh(gen.build(key), palette);
				t.place(m, key);
				const Tiling::Vec mid = t.middle(key);
				for (const Mesh::Part &part : m.parts) {
					auto at = [&](u16 k) {
						const v3f p = part.vertices[k].Pos / BS;
						return Tiling::Vec{ mid.x + p.X, mid.y + p.Y, mid.z + p.Z };
					};
					for (size_t k = 0; k + 2 < part.indices.size(); k += 3) {
						const Tri tri{ at(part.indices[k]), at(part.indices[k + 1]), at(part.indices[k + 2]) };
						const Tiling::Vec d[3] = { unit(tri.a), unit(tri.b), unit(tri.c) };
						int lo[3], hi[3];
						for (int a = 0; a < 3; a++) {
							auto c = [&](const Tiling::Vec &v) { return a == 0 ? v.x : a == 1 ? v.y : v.z; };
							lo[a] = binOf(std::min({ c(d[0]), c(d[1]), c(d[2]) })) - 1; // a bin round: the triangle bows out between its corners
							hi[a] = binOf(std::max({ c(d[0]), c(d[1]), c(d[2]) })) + 1;
						}
						const size_t index = tris.size();
						tris.push_back(tri);
						for (int bx = lo[0]; bx <= hi[0]; bx++)
							for (int by = lo[1]; by <= hi[1]; by++)
								for (int bz = lo[2]; bz <= hi[2]; bz++)
									bins[{ bx, by, bz }].push_back(index);
					}
				}
			}
	}

	// whether the ray from `o` along `d` crosses the triangle (Möller–Trumbore)
	auto crosses = [](const Tiling::Vec &o, const Tiling::Vec &d, const Tri &tri) {
		const Tiling::Vec e1{ tri.b.x - tri.a.x, tri.b.y - tri.a.y, tri.b.z - tri.a.z },
				e2{ tri.c.x - tri.a.x, tri.c.y - tri.a.y, tri.c.z - tri.a.z };
		const Tiling::Vec p{ d.y * e2.z - d.z * e2.y, d.z * e2.x - d.x * e2.z, d.x * e2.y - d.y * e2.x };
		const double det = e1.x * p.x + e1.y * p.y + e1.z * p.z;
		if (std::fabs(det) < 1e-12)
			return false;
		const Tiling::Vec s{ o.x - tri.a.x, o.y - tri.a.y, o.z - tri.a.z };
		const double u = (s.x * p.x + s.y * p.y + s.z * p.z) / det;
		if (u < 0 || u > 1)
			return false;
		const Tiling::Vec q{ s.y * e1.z - s.z * e1.y, s.z * e1.x - s.x * e1.z, s.x * e1.y - s.y * e1.x };
		const double v = (d.x * q.x + d.y * q.y + d.z * q.z) / det;
		if (v < 0 || u + v > 1)
			return false;
		return (e2.x * q.x + e2.y * q.y + e2.z * q.z) / det > 0;
	};

	// Rays on a Fibonacci sphere at the sea: straight down, and slanting 40°
	// (a slit between faces shows only from the side). Each from well
	// outside the land; it must meet a triangle.
	const int RAYS = 40000;
	int holes = 0;
	for (int r = 0; r < RAYS; r++) {
		const double y = 1 - 2 * (r + 0.5) / RAYS, ring = std::sqrt(1 - y * y), a = r * M_PI * (3 - std::sqrt(5.0));
		const Tiling::Vec up{ ring * std::cos(a), y, ring * std::sin(a) };
		// a direction along the ground, turning ray to ray
		Tiling::Vec side{ -up.z, 0, up.x };
		if (std::fabs(ring) < 1e-6)
			side = { 1, 0, 0 };
		side = unit(side);
		const Tiling::Vec north{ up.y * side.z - up.z * side.y, up.z * side.x - up.x * side.z, up.x * side.y - up.y * side.x };
		const double turn = r * 2.399963;
		const Tiling::Vec along{ side.x * std::cos(turn) + north.x * std::sin(turn), side.y * std::cos(turn) + north.y * std::sin(turn),
			side.z * std::cos(turn) + north.z * std::sin(turn) };
		const double slant = (r % 2) ? 40 * M_PI / 180 : 0;
		const Tiling::Vec d{ -up.x * std::cos(slant) + along.x * std::sin(slant), -up.y * std::cos(slant) + along.y * std::sin(slant),
			-up.z * std::cos(slant) + along.z * std::sin(slant) };
		const Tiling::Vec at{ up.x * radius, up.y * radius, up.z * radius };
		const double back = 400; // blocks: above any land and its trees
		const Tiling::Vec o{ at.x - d.x * back, at.y - d.y * back, at.z - d.z * back };
		// the bins the ray passes through, from its start to under the sea
		std::set<std::tuple<int, int, int>> passed;
		for (double along = 0; along <= back + 100; along += BIN * radius / 2) {
			const Tiling::Vec p{ o.x + d.x * along, o.y + d.y * along, o.z + d.z * along };
			const Tiling::Vec u = unit(p);
			passed.insert({ binOf(u.x), binOf(u.y), binOf(u.z) });
		}
		bool hit = false;
		for (const auto &key : passed) {
			const auto bin = bins.find(key);
			if (bin != bins.end())
				for (size_t k : bin->second)
					if ((hit = crosses(o, d, tris[k])))
						break;
			if (hit)
				break;
		}
		if (!hit) {
			if (holes < 12) {
				const terragen::Atlas::Cell c = t.atlas().owner({ at.x, at.y, at.z });
				printf("  hole at lat %.2f lon %.2f, %s (region %d/%d, cell %lld %lld)\n", std::asin(y) * 180 / M_PI,
						std::atan2(up.z, up.x) * 180 / M_PI, slant > 0 ? "slanting" : "straight down", (int)c.region,
						c.index, (long long)c.i, (long long)c.j);
			}
			holes++;
		}
	}
	printf("  radius %.0f, %d levels under the roots: %zu triangles, %d of %d rays through a hole\n", radius, drop,
			tris.size(), holes, RAYS);
	return holes;
}

void TestFar::testNoHolesFromAbove()
{
	set_light_curve(1.0f);
	// the coarsest tiles, where the seams' gaps were widest, and finer; a large planet's
	for (const auto &[params, drop] : { std::pair{ ATLAS, 0 }, std::pair{ ATLAS, 1 },
				 std::pair{ terragen::Atlas::standard(250000), 0 } })
		UTEST(holesFromAbove(params, drop) == 0, "radius %.0f, %d levels down: rays from above find holes",
				params.radius, drop);
}
