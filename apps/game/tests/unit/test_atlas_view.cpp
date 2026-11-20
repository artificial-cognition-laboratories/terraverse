// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tests/unit/test.h"

#include "src/client/views/game/sky/atlas_view.h"
#include "src/shared/base/constants.h"
#include "src/shared/util/math/numeric.h"
#include <map>
#include <set>

/*
	The client's view of an atlas (client/atlas_view.h): a block's matrix
	puts its nodes where the eye's grid has them: the eye's own blocks
	where they are, a block over a seam a step from the node it borders.
*/
class TestAtlasView : public TestBase {
public:
	TestAtlasView() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestAtlasView"; }

	void runTests(IGameDef *gamedef);

	void testOwnBlocksStay();
	void testOverASeam();
	void testTurnedGridIsTurned();
	void testEveryBlockLandsWhereItIs();
	void testAnotherBodyIsntDrawn();
};

static TestAtlasView g_test_instance;

namespace {

const terragen::Atlas::Params PLANET = terragen::Atlas::standard(8192 / (2 * M_PI));

// Where a node's centre is drawn, seen from an eye at `eye` (nodes),
// relative to the eye (nodes).
v3f drawn(const planet::Atlas &atlas, AtlasView &view, v3s16 node, v3f eye)
{
	const v3s16 offset = floatToInt(eye * BS, BS);
	view.update(eye * BS, eye * BS, offset);
	const v3s16 block = getContainerPos(node, MAP_BLOCKSIZE);
	const core::matrix4 *m = view.transform(block);
	if (!m)
		throw std::runtime_error("no transform for a block of cells");
	// the node's centre in its block's mesh: on the planet, from the block's middle
	const auto chart = *atlas.blockChart({ block.X, block.Y, block.Z });
	const terragen::Atlas::Vec p = atlas.onPlanet(chart, node.X, node.Y, node.Z);
	v3f out;
	m->transformVect(out, v3f(p.x, p.y, p.z) * BS);
	return out / BS + intToFloat(offset, 1) - eye;
}

} // namespace

void TestAtlasView::runTests(IGameDef *gamedef)
{
	TEST(testOwnBlocksStay);
	TEST(testOverASeam);
	TEST(testTurnedGridIsTurned);
	TEST(testEveryBlockLandsWhereItIs);
	TEST(testAnotherBodyIsntDrawn);
}

// The eye's own grid round it is drawn as stored.
void TestAtlasView::testOwnBlocksStay()
{
	const planet::Atlas atlas = planet::Atlas::resident(PLANET);
	AtlasView view(atlas);
	const planet::Atlas::Node n = atlas.node(planet::Atlas::Cell(planet::Atlas::HOME, atlas.body(planet::Atlas::HOME).locate({ PLANET.radius + 3.5, 100, 200 })));
	const v3f eye(n.x + 0.2f, n.y + 0.3f, n.z - 0.1f);
	for (const v3s16 d : { v3s16(0, 0, 0), v3s16(5, 0, 0), v3s16(0, 3, -4), v3s16(-6, -2, 7) }) {
		const v3s16 node(n.x + d.X, n.y + d.Y, n.z + d.Z);
		if (!atlas.cell({ node.X, node.Y, node.Z }))
			continue;
		const v3f at = drawn(atlas, view, node, eye), want = intToFloat(node, 1) - eye;
		UTEST(at.getDistanceFrom(want) < 0.05f, "own node drawn %.3f off", at.getDistanceFrom(want));
	}
}

// Over a seam (a few regions' edges, at the surface and near the core): the
// node a step across is drawn a step from the node it borders, within its
// half diagonal.
void TestAtlasView::testOverASeam()
{
	const planet::Atlas atlas = planet::Atlas::resident(PLANET);
	AtlasView view(atlas);
	int crossed = 0;
	std::vector<v3s16> edges;
	for (size_t r = 0; r < atlas.tiles().size(); r += 5) {
		const planet::Atlas::Tile &region = atlas.tiles()[r];
		const planet::Atlas::Rect &rect = region.rect;
		// at the surface, and low (near the core)
		for (const s16 y : { (s16)0, (s16)(region.k0 + 8) })
		for (const v3s16 n : { v3s16(rect.x + rect.width / 2, y, rect.z + rect.depth / 2),
					v3s16(rect.x + rect.width / 4, y, rect.z + rect.depth / 3) }) {
			// out from the middle east and north to the region's edge: the last cell
			for (int axis : { 0, 2 }) {
				v3s16 at = n;
				while (atlas.cell({ at.X, at.Y, at.Z }) && !atlas.step({ at.X, at.Y, at.Z }, axis, 1))
					(axis == 0 ? at.X : at.Z)++;
				if (!atlas.cell({ at.X, at.Y, at.Z }))
					continue;
				edges.push_back(at);
			}
		}
	}
	for (const v3s16 n : edges)
		for (int axis = 0; axis < 3; axis++) // over the seam, and along it
			for (int sign : { -1, 1 }) {
				const auto s = atlas.step({ n.X, n.Y, n.Z }, axis, sign);
				if (!s)
					continue;
				crossed++;
				const v3f eye(n.X + 0.1f, n.Y + 0.2f, n.Z + 0.3f);
				v3f want = intToFloat(n, 1) - eye;
				(axis == 0 ? want.X : axis == 1 ? want.Y : want.Z) += sign * 0.501f;
				const v3f at = drawn(atlas, view, v3s16(s->node.x, s->node.y, s->node.z), eye);
				// a cut cell (either side) is drawn whole: its neighbour past the
				// cut's edge can be a little further than the face (Atlas::through)
				const bool cut = atlas.body(planet::Atlas::HOME).retained(*atlas.cell({ n.X, n.Y, n.Z })) < 1 ||
						atlas.body(planet::Atlas::HOME).retained(*atlas.cell(s->node)) < 1;
				UTEST(at.getDistanceFrom(want) < (cut ? 1.6f : 1.0f), "over a seam drawn %.3f from the face",
						at.getDistanceFrom(want));
			}
	UTEST(crossed > 20, "only %d crossings", crossed);
}

// A cap's grid meets a segment's turned (by the segment's longitude): seen
// from the segment, one step along the cap's x is drawn turned, exactly as
// the atlas's view of it (a transposed matrix would turn it the other way).
void TestAtlasView::testTurnedGridIsTurned()
{
	const planet::Atlas atlas = planet::Atlas::resident(PLANET);
	AtlasView view(atlas);
	const terragen::Atlas &geometry = atlas.body(planet::Atlas::HOME);
	// north up a segment at longitude 52.5° (its middle) to the cap's circle
	const double R = PLANET.radius + 0.5, lon = 52.5 * M_PI / 180;
	planet::Atlas::Node n = atlas.node(planet::Atlas::Cell(planet::Atlas::HOME, geometry.locate({ R * std::cos(lon), 0, R * std::sin(lon) })));
	while (!atlas.step(n, 2, 1))
		n.z++;
	UASSERT(atlas.cell(n).has_value() && atlas.cell(n)->region == terragen::Atlas::Region::Segment);
	const auto s = atlas.step(n, 2, 1);
	UASSERT(s.has_value());
	const v3s16 a(s->node.x, s->node.y, s->node.z), b(a.X + 1, a.Y, a.Z);
	UASSERT(atlas.cell({ b.X, b.Y, b.Z }).has_value());
	const v3f eye(n.x, n.y, n.z);
	const v3f step = drawn(atlas, view, b, eye) - drawn(atlas, view, a, eye);

	const auto eyeFrame = *atlas.frame(n, { eye.X, eye.Y, eye.Z });
	const auto m = planet::Atlas::view(eyeFrame, *atlas.frame(s->node, { (double)a.X, (double)a.Y, (double)a.Z }));
	const v3f want((f32)m.a[0], (f32)m.a[3], (f32)m.a[6]); // the map's first column
	UTEST(step.getDistanceFrom(want) < 0.01f, "the cap's x drawn (%.3f, %.3f, %.3f), wanted (%.3f, %.3f, %.3f)",
			step.X, step.Y, step.Z, want.X, want.Y, want.Z);
	UTEST(std::abs(step.X - 1) > 0.2f, "the cap's grid isn't turned (%.3f)", step.X);
}

/*
	The whole view from an eye, as the client would draw it: every block
	round it (out to a render range, over every seam, through every shell
	floor), every cell of each. A cell must be drawn where it truly is,
	seen flat from the eye: its centre on the planet, less the eye's, in
	the eye's axes. Checked against the planet's geometry directly, not
	the view's own map; and no two cells drawn in one place.
*/
void TestAtlasView::testEveryBlockLandsWhereItIs()
{
	using V = terragen::Atlas::Vec;
	const planet::Atlas atlas = planet::Atlas::resident(PLANET);
	const terragen::Atlas &geometry = atlas.body(planet::Atlas::HOME);
	const double R = PLANET.radius;
	struct Eye { const char *name; V at; };
	const s16 range = 48;
	int worst_eye = 0;
	for (const Eye &e : { Eye{ "band", { R + 3.5, 100, 200 } }, Eye{ "equator", { R + 2.5, 0.4, 0.3 } },
				Eye{ "high band", { (R + 3.5) * 0.3, (R + 3.5) * 0.9, (R + 3.5) * 0.3 } },
				Eye{ "pole", { 0.3, R + 3.5, 0.2 } } }) {
		const double len = std::sqrt(e.at.x * e.at.x + e.at.y * e.at.y + e.at.z * e.at.z);
		const V where{ e.at.x / len * (R + 3.5), e.at.y / len * (R + 3.5), e.at.z / len * (R + 3.5) };
		const planet::Atlas::Node n = atlas.node(planet::Atlas::Cell(planet::Atlas::HOME, geometry.locate(where)));
		const v3f eye(n.x, n.y + 0.3f, n.z);
		AtlasView view(atlas);
		const v3s16 offset = floatToInt(eye * BS, BS);
		view.update(eye * BS, eye * BS, offset);
		const auto ef = *atlas.frame(n, { eye.X, eye.Y, eye.Z });

		// the blocks round it, as the server finds them
		std::set<std::array<s16, 3>> blocks;
		for (s16 z = -range; z <= range; z += 4)
			for (s16 y = -range; y <= range; y += 4)
				for (s16 x = -range; x <= range; x += 4) {
					const auto s = atlas.seeAt(n, n.x + x, n.y + y, n.z + z);
					if (!s || !atlas.cell(s->node))
						continue;
					const v3s16 b = getContainerPos(v3s16(s->node.x, s->node.y, s->node.z), MAP_BLOCKSIZE);
					blocks.insert({ b.X, b.Y, b.Z });
				}

		// the eye's axes, inverted: a true displacement in the eye's grid
		const auto &je = ef.axes;
		const auto cross = [](V a, V b) { return V{ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; };
		const auto dot = [](V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
		const V r0 = cross(je[1], je[2]), r1 = cross(je[2], je[0]), r2 = cross(je[0], je[1]);
		const double det = dot(je[0], r0);

		int checked = 0, bad = 0;
		double worst = 0;
		std::map<std::array<int, 3>, planet::Atlas::Node> taken; // drawn centres, to 1/2 node
		int overlaps = 0, mirrored = 0;
		for (const auto &bp : blocks) {
			const v3s16 block(bp[0], bp[1], bp[2]);
			const core::matrix4 *m = view.transform(block);
			const auto chart = atlas.blockChart({ block.X, block.Y, block.Z });
			if (m) {
				// drawn inside out if the map mirrors: faces wound backwards
				const f32 *a = m->pointer();
				const f32 det = a[0] * (a[5] * a[10] - a[6] * a[9]) - a[4] * (a[1] * a[10] - a[2] * a[9]) +
						a[8] * (a[1] * a[6] - a[2] * a[5]);
				if (det <= 0 && mirrored++ < 5)
					rawstream << e.name << ": block (" << block.X << "," << block.Y << "," << block.Z
						<< ") mirrored, det " << det << std::endl;
			}
			for (s16 z = 0; z < MAP_BLOCKSIZE; z++)
				for (s16 y = 0; y < MAP_BLOCKSIZE; y++)
					for (s16 x = 0; x < MAP_BLOCKSIZE; x++) {
						const v3s16 node = block * MAP_BLOCKSIZE + v3s16(x, y, z);
						const auto c = atlas.cell({ node.X, node.Y, node.Z });
						if (!c)
							continue;
						UTEST(m != nullptr, "%s: block (%d,%d,%d) of cells has no transform", e.name, block.X, block.Y, block.Z);
						if (!m)
							break;
						v3f out;
						const terragen::Atlas::Vec pv = atlas.onPlanet(*chart, node.X, node.Y, node.Z);
						m->transformVect(out, v3f(pv.x, pv.y, pv.z) * BS);
						const v3f drawn = out / BS + intToFloat(offset, 1) - eye;
						const V p = geometry.centre(*c);
						const V d{ p.x - ef.origin.x, p.y - ef.origin.y, p.z - ef.origin.z };
						const v3f want((f32)(dot(r0, d) / det), (f32)(dot(r1, d) / det), (f32)(dot(r2, d) / det));
						const double off = drawn.getDistanceFrom(want);
						checked++;
						if (off > 0.25) {
							if (bad++ < 5)
								rawstream << e.name << ": cell at node (" << node.X << "," << node.Y << "," << node.Z
									<< ") region " << (int)c->region << " " << c->index << " drawn " << off << " off" << std::endl;
						}
						worst = std::max(worst, off);
						// two cells in one place: never, but where cut cells (drawn
						// whole, reaching past their territory) overlap at a seam
						if (want.getLength() < range && atlas.body(planet::Atlas::HOME).retained(*c) == 1) {
							const std::array<int, 3> k{ (int)std::lround(drawn.X * 2), (int)std::lround(drawn.Y * 2),
								(int)std::lround(drawn.Z * 2) };
							const auto [it, fresh] = taken.emplace(k, planet::Atlas::Node{ node.X, node.Y, node.Z });
							if (!fresh && overlaps++ < 5)
								rawstream << e.name << ": nodes (" << node.X << "," << node.Y << "," << node.Z << ") and ("
									<< it->second.x << "," << it->second.y << "," << it->second.z << ") drawn in one place" << std::endl;
						}
					}
		}
		rawstream << e.name << ": " << blocks.size() << " blocks, " << checked << " cells, worst " << worst
			<< ", overlaps " << overlaps << std::endl;
		UTEST(bad == 0, "%s: %d of %d cells drawn away from where they are (worst %.3f)", e.name, bad, checked, worst);
		UTEST(overlaps == 0, "%s: %d cells drawn on another", e.name, overlaps);
		UTEST(mirrored == 0, "%s: %d blocks drawn mirrored (inside out)", e.name, mirrored);
		worst_eye++;
	}
}

// Another body's tile in the next slot: its blocks are nowhere in the
// eye's space, not drawn; the eye's own body's are.
void TestAtlasView::testAnotherBodyIsntDrawn()
{
	planet::Atlas atlas({ PLANET, PLANET });
	// a place a little north of the equator, mid segment 0: the same tile of each body, side by side
	const double lat = 0.05, lon = 7.5 * M_PI / 180, r = PLANET.radius + 2.5;
	const planet::Atlas::Cell c(planet::Atlas::HOME, atlas.body(planet::Atlas::HOME).locate(
			{ r * std::cos(lat) * std::cos(lon), r * std::sin(lat), r * std::cos(lat) * std::sin(lon) }));
	const planet::Atlas::TileId t = atlas.tileOf(c);
	atlas.put(0, t);
	atlas.put(1, planet::Atlas::TileId(1, t.region, t.ti, t.tj));
	const planet::Atlas::Node n = atlas.node(c);
	AtlasView view(atlas);
	const v3f eye(n.x * BS, n.y * BS, n.z * BS);
	view.update(eye, eye, v3s16(0, 0, 0));
	UASSERT(view.eye() && view.eye()->body == planet::Atlas::HOME);
	const v3s16 home(n.x / MAP_BLOCKSIZE, n.y / MAP_BLOCKSIZE, n.z / MAP_BLOCKSIZE);
	const v3s16 other((n.x + planet::Atlas::SLOT) / MAP_BLOCKSIZE, n.y / MAP_BLOCKSIZE, n.z / MAP_BLOCKSIZE);
	UASSERT(*atlas.bodyAt({ n.x + planet::Atlas::SLOT, n.y, n.z }) == 1);
	UASSERT(view.transform(home) != nullptr);
	UASSERT(view.transform(other) == nullptr);
}
