// Terraverse: the checks' probes of an atlas planet (sprint 119: were Lua mods in
// scripts/atlas-*-check). See probe.h.
#include "tests/probe/probe.h"

#include "src/shared/map/light/daynightratio.h"
#include "src/server/world/load/emerge.h"
#include "src/shared/map/map.h"
#include "src/shared/game/content/nodedef.h"
#include "src/server/app/server.h"
#include "src/server/world/terra.h"
#include "src/server/world/state/environment/serverenvironment.h"
#include <chrono>
#include <cmath>
#include <map>
#include <memory>

namespace probe {

namespace {

constexpr double DEG = M_PI / 180;

std::string str(v3s16 p)
{
	return format("(%d,%d,%d)", p.X, p.Y, p.Z);
}

// A box of nodes, inclusive.
struct Box
{
	std::string name;
	v3s16 min, max;
	bool terrain = false; // at the equator's sea level: ground and air are tallied
};

Box around(v3s16 p, const std::string &name, s16 dy)
{
	return { name, p - v3s16(16, dy, 16), p + v3s16(15, dy, 15) };
}

// What's checked, and the first misses said.
struct Tally
{
	int pass = 0, fail = 0;
	void check(Context &c, bool ok, const std::string &what)
	{
		if (ok) {
			pass++;
			return;
		}
		if (++fail <= 20)
			c.say("ATLAS miss " + what);
	}
};

// core.get_spawn_level: the layer one stands at over a column; none where no one spawns.
std::optional<int> spawnLevel(Context &c, s16 x, s16 z)
{
	const int level = c.server()->getEmergeManager()->getSpawnLevelAtPoint(v2s16(x, z));
	if (level >= MAX_MAP_GENERATION_LIMIT)
		return std::nullopt;
	return level;
}

// A node on the equator at a coast: the first step east from land into water, narrowed by
// halving to within a block, since the box round it is only 32 across.
v3s16 coast(Context &c, const std::function<v3s16(double, double, s64)> &at)
{
	const auto land = [&](double lon) {
		const v3s16 p = at(0, lon, 0);
		return spawnLevel(c, p.X, p.Z).has_value();
	};
	for (int lon = 0; lon < 360; lon++) {
		if (!land(lon) || land(lon + 1))
			continue;
		double dry = lon, wet = lon + 1;
		for (int i = 0; i < 40; i++) {
			const double mid = (dry + wet) / 2;
			(land(mid) ? dry : wet) = mid;
		}
		return at(0, dry, 0);
	}
	throw BaseException("atlas world probe: no coast on the equator");
}

// Each box emerged, inspected, then `done`.
void emergeAll(Context &c, const std::vector<Box> &boxes, std::function<void(const Box &)> inspect,
		std::function<void(double)> done)
{
	auto left = std::make_shared<size_t>(boxes.size());
	const auto started = std::chrono::steady_clock::now();
	for (const Box &b : boxes)
		c.emerge(b.min, b.max, [b, inspect, done, left, started] {
			inspect(b);
			if (--*left > 0)
				return;
			done(std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count());
		});
}

} // namespace

// apps/game/tests/checks/atlas-debug.sh (sprint 30): the debug sphere, generated: boxes where things
// meet, every node checked (above the ground air in full sun, the ground painted, storage
// that's no cell seam), and every paint somewhere.
void atlasDebug(Context &c)
{
	auto t = std::make_shared<Tally>();
	auto paints = std::make_shared<std::map<std::string, int>>();
	const auto at = [&c](double lat, double lon, int layer) { return *c.atlasNode(lat * DEG, lon * DEG, layer); };
	const std::vector<Box> boxes = {
		around(at(0, 3, 0), "the equator, its surface and below", 24),
		around(at(60, 15, 0), "a segment's seam near 60°", 6),
		around(at(85.2, 30, 0), "the north cap's edge", 6),
		around(at(-85.2, 30, 0), "the south cap's edge", 6),
	};
	const auto inspect = [&c, t, paints](const Box &box) {
		const NodeDefManager *ndef = c.server()->ndef();
		const u32 noon = time_to_daynight_ratio(12000, true);
		for (s16 z = box.min.Z; z <= box.max.Z; z++)
			for (s16 y = box.min.Y; y <= box.max.Y; y++)
				for (s16 x = box.min.X; x <= box.max.X; x++) {
					const v3s16 p(x, y, z);
					const auto cell = c.cell(p);
					const MapNode n = c.env().getMap().getNode(p);
					const std::string name = ndef->get(n).name;
					if (!cell) {
						t->check(c, name == "core:seam", box.name + ": no cell, but " + name);
					} else if (cell->k >= 0) {
						const int light = n.getLightBlend(noon, ndef->getLightingFlags(n));
						t->check(c, name == "air" && light == 15, format("%s: layer %lld at %s is %s lit %d",
								box.name.c_str(), (long long)cell->k, str(p).c_str(), name.c_str(), light));
					} else {
						const std::string prefix = "core:atlas_debug_";
						const bool painted = name.rfind(prefix, 0) == 0;
						t->check(c, painted, format("%s: ground at %s is %s", box.name.c_str(), str(p).c_str(), name.c_str()));
						if (painted)
							(*paints)[name.substr(prefix.size())]++;
					}
				}
	};
	c.after(0, [&c, t, paints, boxes, inspect] {
		emergeAll(c, boxes, inspect, [&c, t, paints, n = boxes.size()](double took) {
			c.say(format("ATLAS generated %d boxes in %.1f s", (int)n, took));
			for (const char *p : { "segment_a", "segment_b", "cap", "segment_seam", "cap_edge", "cut", "line" }) {
				const int count = paints->count(p) ? paints->at(p) : 0;
				c.say(format("ATLAS paint %s %d", p, count));
				t->check(c, count > 0, std::string("no ") + p + " painted");
			}
			const v3s16 spawn = c.spawn();
			const auto cell = c.cell(spawn);
			const auto level = spawnLevel(c, spawn.X, spawn.Z);
			t->check(c, cell && cell->k == 0, format("the spawn %s is at layer %s, not 0 (spawn level %s)",
					str(spawn).c_str(), cell ? std::to_string(cell->k).c_str() : "nil",
					level ? std::to_string(*level).c_str() : "nil"));
			c.say(format("ATLAS pass %d fail %d", t->pass, t->fail));
			c.finish();
		});
	});
}

// apps/game/tests/checks/atlas-world.sh (sprint 30): an atlas world, generated: boxes where the
// atlas's storage changes emerged, then every node checked: a node that stores a cell is
// generated content, never seam or ignore; one that stores no cell is seam. Then the land
// along the equator, and a new player's spawn.
void atlasWorld(Context &c)
{
	const std::string SEAM = "core:seam";
	auto t = std::make_shared<Tally>();
	struct Seen { std::map<std::string, int> cells, kinds; int seam = 0; };
	auto seen = std::make_shared<Seen>();
	const auto at = [&c](double lat, double lon, s64 layer) { return *c.atlasNode(lat * DEG, lon * DEG, layer); };
	const planet::Atlas &atlas = c.atlas();
	const terragen::Atlas::Params &params = atlas.body(planet::Atlas::HOME).params();
	// a segment's box's right edge at the surface: the gap beside it; the core's box's middle
	std::optional<v3s16> edge, inner;
	for (const planet::Atlas::Tile &r : atlas.tiles()) {
		if (r.kind == terragen::Atlas::Region::Segment && !edge)
			edge = v3s16(r.rect.x + r.rect.width, 0, r.rect.z + r.rect.depth / 2);
		if (r.kind == terragen::Atlas::Region::Core)
			inner = v3s16(r.rect.x + r.rect.width / 2, 0, r.rect.z + r.rect.depth / 2);
	}
	if (!inner || !edge)
		throw BaseException("atlas world probe: no core, or no segment");
	const double capLat = 90 - 5; // the cap's edge at the default 85°: a box straddling it
	// on the equator at a coast, found rather than named (a fixed longitude goes to sea whenever
	// the land changes), so ground and air both meet the box at sea level
	const v3s16 equator = coast(c, at);
	Box first{ "the equator, its surface and below", v3s16(equator.X - 16, -24, equator.Z - 16),
		v3s16(equator.X + 15, 24, equator.Z + 15), true };
	const std::vector<Box> boxes = {
		first,
		around(at(60, 15, 0), "a segment's seam near 60°", 8),
		around(at(capLat + 0.2, 30, 0), "the north cap's edge", 8),
		around(at(-capLat - 0.2, 30, 0), "the south cap's edge", 8),
		around(*edge, "a region's edge and the gap", 8),
		around(at(45, 10, (s64)std::floor(params.core - params.radius) + 4), "a segment's floor at the core's sphere", 8),
		Box{ "the core's own grid", v3s16(inner->X - 16, -8, inner->Z - 16), v3s16(inner->X + 15, 8, inner->Z + 15) },
	};
	const auto inspect = [&c, t, seen, SEAM](const Box &box) {
		for (s16 z = box.min.Z; z <= box.max.Z; z++)
			for (s16 y = box.min.Y; y <= box.max.Y; y++)
				for (s16 x = box.min.X; x <= box.max.X; x++) {
					const v3s16 p(x, y, z);
					const auto cell = c.cell(p);
					const std::string name = c.node(p);
					if (cell) {
						seen->cells[regionName(cell->region)]++;
						if (box.terrain)
							seen->kinds[name == "air" ? "air" : c.group(name, "liquid") > 0 ? "liquid" : "solid"]++;
						t->check(c, name != SEAM && name != "ignore", format("%s: cell %s at %s is %s", box.name.c_str(),
								regionName(cell->region), str(p).c_str(), name.c_str()));
					} else {
						seen->seam++;
						t->check(c, name == SEAM, format("%s: no cell at %s, but %s", box.name.c_str(), str(p).c_str(), name.c_str()));
					}
				}
	};
	// A new player's spawn: a cell, open, over ground within three layers below it.
	const auto spawn = [&c, t](std::function<void()> done) {
		const v3s16 pos = c.spawn();
		const auto cell = c.cell(pos);
		t->check(c, cell.has_value(), "spawn " + str(pos) + " is seam");
		if (!cell)
			return done();
		const auto [lat, lon] = *c.place(pos);
		std::vector<v3s16> below;
		for (int d = 1; d <= 3; d++)
			below.push_back(*c.atlasNode(lat, lon, cell->k - d));
		auto left = std::make_shared<int>(4);
		for (const v3s16 &p : { pos, below[0], below[1], below[2] })
			c.emerge(p, p, [&c, t, pos, below, cell, left, done] {
				if (--*left > 0)
					return;
				const std::string here = c.node(pos);
				std::string under;
				bool ground = false;
				for (const v3s16 &b : below) {
					const std::string n = c.node(b);
					under += (under.empty() ? "" : ", ") + n;
					ground = ground || c.def(n).walkable;
				}
				c.say(format("ATLAS spawn %s layer %lld: %s over %s", str(pos).c_str(), (long long)cell->k, here.c_str(), under.c_str()));
				t->check(c, !c.def(here).walkable, "spawn is inside " + here);
				t->check(c, ground, "nothing to stand on under the spawn");
				done();
			});
	};
	c.after(0, [&c, t, seen, boxes, inspect, spawn, at] {
		emergeAll(c, boxes, inspect, [&c, t, seen, spawn, at, n = boxes.size()](double took) {
			c.say(format("ATLAS generated %d boxes in %.1f s", (int)n, took));
			for (const auto &[region, count] : seen->cells)
				c.say(format("ATLAS cells %s %d", region.c_str(), count));
			c.say(format("ATLAS cells seam %d", seen->seam));
			for (const char *region : { "segment", "cap", "core" })
				t->check(c, seen->cells[region] > 0, std::string("no ") + region + " cells seen");
			t->check(c, seen->seam > 0, "no seam seen");
			// the terrain is real: ground and air at the equator's sea level
			for (const char *kind : { "solid", "air" }) {
				c.say(format("ATLAS equator %s %d", kind, seen->kinds[kind]));
				t->check(c, seen->kinds[kind] > 0, std::string("no ") + kind + " at the equator's sea level");
			}
			// and somewhere along the equator there's land to spawn on
			std::optional<v3s16> land;
			for (int lon = 0; lon <= 359 && !land; lon += 3) {
				const v3s16 p = at(0, lon, 0);
				if (const auto y = spawnLevel(c, p.X, p.Z))
					land = v3s16(p.X, *y, p.Z);
			}
			c.say("ATLAS land on the equator: " + (land ? str(*land) : std::string("none")));
			t->check(c, land.has_value(), "no land anywhere on the equator");
			spawn([&c, t] {
				c.say(format("ATLAS pass %d fail %d", t->pass, t->fail));
				c.finish();
			});
		});
	});
}

// apps/game/tests/checks/atlas-land.sh (planning/sprint-32-atlas-lod.md): what the generator places
// is terra::AtlasLand's, node for node, where the atlas changes. The world may differ only
// by what came after the terrain: the planting (trees, plants, snow) and liquid that flowed.
void atlasLand(Context &c)
{
	const std::string SEAM = "core:seam";
	const auto at = [&c](double lat, double lon, s64 layer) { return *c.atlasNode(lat * DEG, lon * DEG, layer); };
	const auto box = [](const std::string &name, v3s16 p, s16 below, s16 above, s16 half = 20) {
		return Box{ name, v3s16(p.X - half, p.Y - below, p.Z - half), v3s16(p.X + half - 1, p.Y + above, p.Z + half - 1) };
	};
	const planet::Atlas::Tile *segment = nullptr, *inner = nullptr;
	for (const planet::Atlas::Tile &r : c.atlas().tiles()) {
		if (r.kind == terragen::Atlas::Region::Segment && !segment)
			segment = &r;
		if (r.kind == terragen::Atlas::Region::Core)
			inner = &r;
	}
	if (!segment || !inner)
		throw BaseException("atlas land probe: no segment or core");
	const planet::Atlas::Rect s = segment->rect, k = inner->rect;
	const std::vector<Box> boxes = {
		box("a segment's middle, the surface", at(0, 7.5, 0), 40, 60),
		box("a segment's east edge and the seam", v3s16(s.x + s.width, 0, s.z + s.depth / 2), 30, 40),
		box("a cap's middle", at(89.5, 0, 0), 40, 60),
		box("across the cap circle", at(85, 40, 0), 30, 40),
		box("a segment down by the core", v3s16(s.x + s.width / 2, segment->k0 + 10, s.z + s.depth / 2), 10, 20),
		box("the core", v3s16(k.x + k.width / 2, inner->k0 + (inner->k1 + 1 - inner->k0) / 2, k.z + k.depth / 2), 16, 16, 16),
	};
	// what the game does after the terrain
	const auto later = [&c](const std::string &name) -> std::optional<std::string> {
		if (name.size() > 8 && name.compare(name.size() - 8, 8, "_flowing") == 0)
			return "flowed";
		const ContentFeatures &f = c.def(name);
		for (const char *g : { "tree", "leaves", "plant", "flora", "sapling" })
			if (itemgroup_get(f.groups, g) > 0)
				return "planted";
		if (name.find("snow") != std::string::npos || f.drawtype == NDT_PLANTLIKE || f.drawtype == NDT_NODEBOX)
			return "planted";
		return std::nullopt;
	};
	struct Totals { long nodes = 0, same = 0, planted = 0, flowed = 0, wrong = 0, rises = 0, misplaced = 0; int shown = 0; std::map<std::string, int> pairs; };
	auto totals = std::make_shared<Totals>();
	auto pending = std::make_shared<size_t>(boxes.size());
	c.after(0, [&c, boxes, later, totals, pending, SEAM] {
		for (const Box &b : boxes)
			c.emerge(b.min, b.max, [&c, b, later, totals, pending, SEAM] {
				const NodeDefManager *ndef = c.server()->ndef();
				const std::vector<std::optional<content_t>> land = terraLand(c.server(), b.min, b.max);
				const s64 sx = b.max.X - b.min.X + 1, sy = b.max.Y - b.min.Y + 1;
				long n = 0;
				for (s16 z = b.min.Z; z <= b.max.Z; z++)
					for (s16 y = b.min.Y; y <= b.max.Y; y++)
						for (s16 x = b.min.X; x <= b.max.X; x++) {
							const std::string world = c.node(v3s16(x, y, z));
							const auto &l = land[(z - b.min.Z) * sy * sx + (y - b.min.Y) * sx + (x - b.min.X)];
							const std::string want = l ? ndef->get(*l).name : SEAM;
							n++;
							if (world == want) {
								totals->same++;
								continue;
							}
							if (const auto why = later(world)) {
								(*why == "flowed" ? totals->flowed : totals->planted)++;
								continue;
							}
							totals->wrong++;
							totals->pairs[want + " -> " + world]++;
							if (totals->shown++ < 10)
								c.say(format("LAND wrong in %s at %d %d %d: land %s, world %s", b.name.c_str(), x, y, z,
										want.c_str(), world.c_str()));
						}
				totals->nodes += n;
				// where the ground stands in its top blocks (sprint 128): every
				// rise on a walkable node (loose or rock, sprint 129) with nothing walkable over it
				Map &map = c.env().getMap();
				for (s16 bz = getNodeBlockPos(b.min).Z; bz <= getNodeBlockPos(b.max).Z; bz++)
					for (s16 by = getNodeBlockPos(b.min).Y; by <= getNodeBlockPos(b.max).Y; by++)
						for (s16 bx = getNodeBlockPos(b.min).X; bx <= getNodeBlockPos(b.max).X; bx++) {
							MapBlock *block = map.getBlockNoCreateNoEx(v3s16(bx, by, bz));
							if (!block)
								continue;
							const v3s16 base = block->getPosRelative();
							block->m_rises.each([&](v3s16 rel, int) {
								const v3s16 p = base + rel;
								totals->rises++;
								if (!ndef->get(map.getNode(p)).walkable || ndef->get(map.getNode(p + v3s16(0, 1, 0))).walkable)
									totals->misplaced++;
							});
						}
				c.say(format("LAND box %s: %ld nodes", b.name.c_str(), n));
				if (--*pending > 0)
					return;
				for (const auto &[pair, count] : totals->pairs)
					c.say(format("LAND pair %s: %d", pair.c_str(), count));
				c.say(format("LAND rises %ld misplaced %ld", totals->rises, totals->misplaced));
				c.say(format("LAND nodes %ld same %ld planted %ld flowed %ld wrong %ld", totals->nodes, totals->same,
						totals->planted, totals->flowed, totals->wrong));
				c.finish();
			});
	});
}

} // namespace probe
