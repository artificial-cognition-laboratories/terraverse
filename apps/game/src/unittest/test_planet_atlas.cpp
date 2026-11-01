// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "planet/atlas.h"
#include <algorithm>
#include <cmath>
#include <random>

using planet::Atlas;
using Kind = terragen::Atlas::Region;

/*
	The atlas in the map (layout v2: planet.paper.md, section 2): every
	cell has exactly one node and every node at most one cell; a plain step
	from a stored node is a true neighbour or seam, never another cell; the
	regions keep apart; the planet fits or is refused; a node's column is
	built from the place it is.
*/
class TestPlanetAtlas : public TestBase {
public:
	TestPlanetAtlas() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestPlanetAtlas"; }

	void runTests(IGameDef *gamedef);

	void testFits();
	void testRegionsApart();
	void testNodesAndCellsPair();
	void testPlainStepsNeverLie();
	void testEveryCellIsStored();
	void testPlaceIsTheCell();
	void testStepsCross();
	void testPolewardReachesCaps();
	void testSeeingCarriesTheGrid();
	void testCarryKeepsThePoint();
	void testCarryAboveTheTop();
	void testTurnsFollowTheGround();
	void testViewIsTheGrid();
	void testCutsPartitionThePlanet();
	void testTileMovesWhole();
	void testWindowCode();
	void testStepsBetweenSlots();
	void testTilesNear();
};

static TestPlanetAtlas g_test_instance;

namespace {

// The game's default world (8192 round, 1:1), and a small planet with few,
// wide segments and a thin sky.
std::vector<terragen::Atlas::Params> planets()
{
	return { terragen::Atlas::standard(8192 / (2 * M_PI)), { 300, 12, 80, 60, 0.95, 32 } };
}

terragen::Atlas::Vec randomPoint(const terragen::Atlas &atlas, std::mt19937_64 &rng)
{
	std::normal_distribution<double> normal;
	const auto &p = atlas.params();
	std::uniform_real_distribution<double> radius(1, p.radius + p.height - 1e-6);
	double x = normal(rng), y = normal(rng), z = normal(rng);
	const double r = radius(rng) / std::sqrt(x * x + y * y + z * z);
	return { x * r, y * r, z * r };
}

/*
	The nodes worth visiting: in every region, at its lowest layers (by the
	core), the surface and its top layer, the region's edge rows and
	columns with a node of seam round them, and every 5th row and 29th
	column between.
*/
template <typename Visit>
void sweep(const Atlas &a, Visit visit)
{
	for (const Atlas::Tile &r : a.tiles())
		for (int32_t y : { r.k0, r.k0 + 1, std::max(r.k0, std::min(0, r.k1)), r.k1 }) {
			for (int32_t z = r.rect.z - 1; z <= r.rect.z + r.rect.depth; z++) {
				const int32_t row = z - r.rect.z;
				if (row > 2 && row < r.rect.depth - 3 && row % 5)
					continue;
				for (int32_t x = r.rect.x - 1; x <= r.rect.x + r.rect.width; x++) {
					const int32_t from = x - r.rect.x;
					if (from > 3 && from < r.rect.width - 4 && from % 29)
						continue;
					visit(Atlas::Node{ x, y, z });
				}
			}
		}
}

} // namespace

void TestPlanetAtlas::runTests(IGameDef *gamedef)
{
	TEST(testFits);
	TEST(testRegionsApart);
	TEST(testNodesAndCellsPair);
	TEST(testPlainStepsNeverLie);
	TEST(testEveryCellIsStored);
	TEST(testPlaceIsTheCell);
	TEST(testStepsCross);
	TEST(testPolewardReachesCaps);
	TEST(testSeeingCarriesTheGrid);
	TEST(testCarryKeepsThePoint);
	TEST(testCarryAboveTheTop);
	TEST(testTurnsFollowTheGround);
	TEST(testViewIsTheGrid);
	TEST(testCutsPartitionThePlanet);
	TEST(testTileMovesWhole);
	TEST(testWindowCode);
	TEST(testStepsBetweenSlots);
	TEST(testTilesNear);
}

namespace {

// Stored nodes at the edges of regions (where crossings are), across the
// planet: every `stride`th of the sweep.
std::vector<Atlas::Node> edgeNodes(const Atlas &a, size_t stride)
{
	std::vector<Atlas::Node> out;
	size_t n = 0;
	sweep(a, [&](Atlas::Node node) {
		if (n++ % stride == 0 && a.cell(node))
			out.push_back(node);
	});
	return out;
}

const int STEPS[6][2] = { { 0, -1 }, { 0, 1 }, { 1, -1 }, { 1, 1 }, { 2, -1 }, { 2, 1 } }; // axis, sign

} // namespace

void TestPlanetAtlas::testFits()
{
	using terragen::Atlas;
	planet::Atlas::resident(Atlas::standard(8192 / (2 * M_PI)));
	planet::Atlas::resident(Atlas::standard(16384 / (2 * M_PI))); // the largest world
	EXCEPTION_CHECK(std::invalid_argument, planet::Atlas::resident(Atlas::standard(13000)));
}

// The tiles are a gap apart, inside the map.
void TestPlanetAtlas::testRegionsApart()
{
	for (const auto &p : planets()) {
		const Atlas a = Atlas::resident(p);
		const auto &regions = a.tiles();
		for (size_t i = 0; i < regions.size(); i++) {
			const Atlas::Rect &r = regions[i].rect;
			UASSERT(r.x >= Atlas::ORIGIN && r.z >= Atlas::ORIGIN &&
					r.x + r.width - Atlas::ORIGIN <= Atlas::SPAN && r.z + r.depth - Atlas::ORIGIN <= Atlas::SPAN);
			std::vector<Atlas::Rect> others;
			for (size_t j = i + 1; j < regions.size(); j++)
				others.push_back(regions[j].rect);
			for (const Atlas::Rect &s : others) {
				const bool apartX = r.x + r.width + Atlas::GAP <= s.x || s.x + s.width + Atlas::GAP <= r.x;
				const bool apartZ = r.z + r.depth + Atlas::GAP <= s.z || s.z + s.depth + Atlas::GAP <= r.z;
				UASSERT(apartX || apartZ);
			}
		}
	}
}

void TestPlanetAtlas::testNodesAndCellsPair()
{
	for (const auto &p : planets()) {
		const Atlas a = Atlas::resident(p);
		size_t counts[3] = {};
		sweep(a, [&](Atlas::Node n) {
			const std::optional<Atlas::Cell> c = a.cell(n);
			if (!c)
				return;
			counts[(int)c->region]++;
			const Atlas::Node back = a.node(*c);
			UASSERT(back.x == n.x && back.y == n.y && back.z == n.z);
		});
		UASSERT(counts[(int)Kind::Segment] > 0 && counts[(int)Kind::Cap] > 0 && counts[(int)Kind::Core] > 0);
	}
}

/*
	The layout's rule: from any stored node, p + dir is seam or one of the
	cell's true neighbours across that face (terragen::Atlas::neighbours).
	Engine code that steps without Map::next meets the seam, never a wrong
	block.
*/
void TestPlanetAtlas::testPlainStepsNeverLie()
{
	static const int DIRS[6][3] = { { -1, 0, 0 }, { 1, 0, 0 }, { 0, 0, -1 }, { 0, 0, 1 }, { 0, -1, 0 }, { 0, 1, 0 } };
	for (const auto &p : planets()) {
		const Atlas a = Atlas::resident(p);
		size_t crossings = 0;
		sweep(a, [&](Atlas::Node n) {
			const std::optional<Atlas::Cell> c = a.cell(n);
			if (!c)
				return;
			for (int d = 0; d < 6; d++) {
				const Atlas::Node q{ n.x + DIRS[d][0], n.y + DIRS[d][1], n.z + DIRS[d][2] };
				const std::optional<Atlas::Cell> there = a.cell(q);
				if (!there)
					continue;
				// storage axes are the grid's: x i, z j, y k (DIRS in face order)
				const auto truth = a.body(planet::Atlas::HOME).neighbours(*c, d);
				UTEST(std::find(truth.begin(), truth.end(), *there) != truth.end(),
						"(%d,%d,%d) + dir %d is a cell that isn't its neighbour", n.x, n.y, n.z, d);
				crossings++;
			}
		});
		UASSERT(crossings > 10000);
	}
}

// Any cell the atlas can locate has a node.
void TestPlanetAtlas::testEveryCellIsStored()
{
	std::mt19937_64 rng(5);
	for (const auto &p : planets()) {
		const Atlas a = Atlas::resident(p);
		int stored = 0;
		for (int n = 0; n < 20000; n++) {
			const Atlas::Cell c(Atlas::HOME, a.body(planet::Atlas::HOME).locate(randomPoint(a.body(planet::Atlas::HOME), rng)));
			const Atlas::Node node = a.node(c); // throws if not stored
			UASSERT(a.cell(node) && *a.cell(node) == c);
			stored++;
		}
		UASSERT(stored > 1000);
	}
}

// A node's column is built from where it is: place() is the direction of
// its column's cell at the surface.
void TestPlanetAtlas::testPlaceIsTheCell()
{
	std::mt19937_64 rng(6);
	for (const auto &p : planets()) {
		const Atlas a = Atlas::resident(p);
		for (int n = 0; n < 4000; n++) {
			const Atlas::Cell c(Atlas::HOME, a.body(planet::Atlas::HOME).locate(randomPoint(a.body(planet::Atlas::HOME), rng)));
			if (c.region == Kind::Core)
				continue;
			const std::optional<terragen::Place> at = a.place(a.node(c));
			UASSERT(at.has_value());
			const terragen::Atlas::Vec m = a.body(planet::Atlas::HOME).centre({ c.region, c.index, c.i, c.j, 0 });
			const double R = std::sqrt(m.x * m.x + m.y * m.y + m.z * m.z);
			// to a hundredth of a block (it's made to 1/1024 of one)
			const double one = (double)terragen::PLACE_ONE, close = 0.01 / R;
			UASSERT(std::abs(at->x / one - m.x / R) < close);
			UASSERT(std::abs(at->y / one - m.y / R) < close);
			UASSERT(std::abs(at->z / one - m.z / R) < close);
		}
	}
}

// A step across a seam lands on a true neighbour of the cell, and segments
// meet segments unturned (their grids differ by a segment's width).
void TestPlanetAtlas::testStepsCross()
{
	for (const auto &p : planets()) {
		const Atlas a = Atlas::resident(p);
		size_t crossed = 0;
		for (const Atlas::Node n : edgeNodes(a, 3)) {
			const Atlas::Cell c = *a.cell(n);
			for (const auto &[axis, sign] : STEPS) {
				const std::optional<Atlas::Step> s = a.step(n, axis, sign);
				if (!s)
					continue;
				crossed++;
				const Atlas::Cell there = *a.cell(s->node);
				const int face = 2 * (axis == 0 ? 0 : axis == 1 ? 2 : 1) + (sign > 0);
				const auto truth = a.body(planet::Atlas::HOME).neighbours(c, face);
				UTEST(std::find(truth.begin(), truth.end(), there) != truth.end(),
						"step %d,%d from region %d (%lld,%lld,%lld) to region %d: not a neighbour", axis, sign,
						(int)c.region, (long long)c.i, (long long)c.j, (long long)c.k, (int)there.region);
				if (c.region == Kind::Segment && there.region == Kind::Segment)
					UTEST(s->turn == 0, "segments turned %d", s->turn);
				// within its own grid, a step is only ever to the grid's neighbour that way
				if (there.region == c.region && there.index == c.index) {
					Atlas::Cell plain = c;
					(axis == 0 ? plain.i : axis == 1 ? plain.k : plain.j) += sign;
					UTEST(there == plain, "step %d,%d from region %d (%lld,%lld,%lld) to its own grid's (%lld,%lld,%lld)", axis,
							sign, (int)c.region, (long long)c.i, (long long)c.j, (long long)c.k, (long long)there.i,
							(long long)there.j, (long long)there.k);
				}
			}
		}
		UTEST(crossed > 500, "only %zu crossings", crossed);
	}
}

// Walking north (south) along any column of a segment at the surface, the
// step past its last cell lands in the cap.
void TestPlanetAtlas::testPolewardReachesCaps()
{
	for (const auto &p : planets()) {
		const Atlas a = Atlas::resident(p);
		for (const auto &g : a.body(planet::Atlas::HOME).grids()) {
			if (g.region != Kind::Segment)
				continue;
			for (int64_t i = g.low[0]; i <= g.high[0]; i += 7)
				for (const int sign : { 1, -1 }) {
					// the column's last cell poleward
					std::optional<Atlas::Cell> last;
					for (int64_t j = sign > 0 ? g.high[1] : g.low[1]; j != (sign > 0 ? g.low[1] : g.high[1]); j -= sign) {
						const Atlas::Cell c(Atlas::HOME, { Kind::Segment, g.index, i, j, 0 });
						if (a.body(planet::Atlas::HOME).exists(c)) {
							last = c;
							break;
						}
					}
					if (!last)
						continue;
					const std::optional<Atlas::Step> s = a.step(a.node(*last), 2, sign);
					UTEST(s.has_value(), "segment %d column %lld: no step %s past j %lld", g.index, (long long)i,
							sign > 0 ? "north" : "south", (long long)last->j);
					if (!s)
						continue;
					// the columns that reach the cap circle (the wedge's half width
					// there); the others leave it through a neighbouring segment
					const double reach = 0.8 * p.radius * std::cos(p.capLatitude * M_PI / 180) * M_PI / p.segments;
					if (std::abs((double)i) > reach)
						continue;
					const Atlas::Cell there = *a.cell(s->node);
					UTEST(there.region == Kind::Cap && there.index == sign, "segment %d column %lld: stepped %s into region %d",
							g.index, (long long)i, sign > 0 ? "north" : "south", (int)there.region);
				}
		}
	}
}

// Seen from a node, the grid carries on over every seam: what's a step
// away in it is the cell that holds that point of the planet.
void TestPlanetAtlas::testSeeingCarriesTheGrid()
{
	for (const auto &p : planets()) {
		const Atlas a = Atlas::resident(p);
		size_t across = 0;
		for (const Atlas::Node n : edgeNodes(a, 61)) {
			for (int dx = -1; dx <= 1; dx++)
				for (int dz = -1; dz <= 1; dz++)
					for (int dy : { -1, 0, 1 }) {
						const Atlas::Node q{ n.x + dx, n.y + dy, n.z + dz };
						const std::optional<Atlas::Seen> s = a.see(n, q);
						if (!s)
							continue;
						if (!a.cell(s->node))
							continue; // past the top: seam
						across++;
						// the point the grid names is inside the cell seen: within
						// half its diagonal of its centre
						const Atlas::Cell to = *a.cell(s->node);
						const terragen::Atlas::Vec named = *a.point(n, q.x, q.y, q.z), centre = a.body(planet::Atlas::HOME).centre(to);
						const double off = std::sqrt(std::pow(named.x - centre.x, 2) + std::pow(named.y - centre.y, 2) +
								std::pow(named.z - centre.z, 2));
						UTEST(off < 0.9, "seen %.3f blocks off (region %d, kept %.3f)", off, (int)to.region,
								a.body(planet::Atlas::HOME).retained(to));
					}
		}
		UTEST(across > 100, "only %zu seen across", across);
	}
}

// Carried into another grid, a position is the same place on the planet:
// both grids put it at the same point (to a couple of centimetres: see
// Atlas::UP), and it stays put there.
void TestPlanetAtlas::testCarryKeepsThePoint()
{
	std::mt19937_64 rng(7);
	std::uniform_real_distribution<double> wander(-2.4, 2.4);
	for (const auto &p : planets()) {
		const Atlas a = Atlas::resident(p);
		size_t carried = 0;
		double worst = 0;
		for (const Atlas::Node n : edgeNodes(a, 3)) {
			const double x = n.x + wander(rng), y = n.y + wander(rng) / 2, z = n.z + wander(rng);
			const std::optional<Atlas::Carried> to = a.carry(n.x, n.y, n.z, x, y, z);
			if (!to)
				continue;
			carried++;
			// (a step up past the top lands over the top: charted from under it)
			const Atlas::Node m = *a.charted(Atlas::moverNode(to->x, to->y, to->z));
			const terragen::Atlas::Vec before = *a.point(n, x, y, z), after = *a.point(m, to->x, to->y, to->z);
			// and it stays: carried again from where it landed, it doesn't move
			UASSERT(!a.carry(to->x, to->y, to->z, to->x, to->y, to->z).has_value());
			const double gap = std::sqrt(std::pow(before.x - after.x, 2) + std::pow(before.y - after.y, 2) +
					std::pow(before.z - after.z, 2));
			// to within the mover's centimetre up (Atlas::UP), or where it landed
			// on a sliver merged into the cell (terragen::Atlas::locate), onto
			// the cell, by the sliver's thickness
			const Atlas::Cell into = *a.cell(m);
			const auto q = a.body(planet::Atlas::HOME).grid(into.region, into.index, before);
			const bool merged = q[0] < into.i || q[0] >= into.i + 1 || q[1] < into.j || q[1] >= into.j + 1 ||
					q[2] < into.k - 0.02 || q[2] >= into.k + 1.02;
			worst = std::max(worst, gap);
			UTEST(gap < (merged ? 0.25 : 2 * Atlas::UP), "carried %.3g blocks off (%s)", gap,
					merged ? "merged" : "into the cell holding it");
		}
		rawstream << "carried " << carried << ", worst " << worst << " blocks" << std::endl;
		UTEST(carried > 100, "only %zu carried", carried);
	}
}

// Above the atlas's top (an orbit over a low sky), crossing a seam still
// carries: charted from the top cell under it, it lands over a column of
// the new grid and stays; the point is kept, but for the wedges between
// leaning columns (at most 1.5% of the height above the top).
void TestPlanetAtlas::testCarryAboveTheTop()
{
	std::mt19937_64 rng(11);
	std::uniform_real_distribution<double> wander(-2.4, 2.4);
	for (const auto &p : planets()) {
		const Atlas a = Atlas::resident(p);
		const int32_t top = a.body(planet::Atlas::HOME).params().height - 1;
		const double up = 300;
		size_t carried = 0;
		double worst = 0;
		size_t over = 0;
		for (const Atlas::Node n : edgeNodes(a, 1)) {
			if (n.y != top)
				continue;
			const double x = n.x + wander(rng), y = top + up, z = n.z + wander(rng);
			const std::optional<Atlas::Carried> to = a.carry(n.x, top + up, n.z, x, y, z);
			if (!to)
				continue;
			carried++;
			const std::optional<Atlas::Node> m = a.charted(Atlas::moverNode(to->x, to->y, to->z));
			UASSERT(m.has_value());
			UTEST(!a.carry(to->x, to->y, to->z, to->x, to->y, to->z).has_value(), "carried again, %.0f up", to->y);
			const terragen::Atlas::Vec before = *a.point(n, x, y, z), after = *a.point(*m, to->x, to->y, to->z);
			const double gap = std::sqrt(std::pow(before.x - after.x, 2) + std::pow(before.y - after.y, 2) +
					std::pow(before.z - after.z, 2));
			worst = std::max(worst, gap);
			over += gap > 0.5;
			// columns of neighbouring grids lean apart up there, leaving
			// wedges in none: a point in one goes to the nearest column's edge
			UTEST(gap < 0.015 * up, "carried %.3g blocks off, %.0f above the top", gap, up);
		}
		rawstream << "carried above the top " << carried << ", worst " << worst << " blocks, " << over << " over half a block" << std::endl;
		UTEST(carried > 50, "only %zu carried above the top", carried);
	}
}

// Into a cap from a segment, all round the cap's circle: a step north turns
// by the longitude, and Atlas::turned with the turn given is the direction
// in the cap's grid nearest the segment's north on the ground (within 45°).
void TestPlanetAtlas::testTurnsFollowTheGround()
{
	using V = terragen::Atlas::Vec;
	const auto sub = [](V a, V b) { return V{ a.x - b.x, a.y - b.y, a.z - b.z }; };
	const auto unit = [](V a) { const double l = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); return V{ a.x / l, a.y / l, a.z / l }; };
	for (const auto &p : planets()) {
		const Atlas a = Atlas::resident(p);
		const terragen::Atlas &atlas = a.body(planet::Atlas::HOME);
		const double R = p.radius + 0.5, lat = (p.capLatitude - 1) * M_PI / 180;
		int turned = 0, entered = 0;
		for (int deg = 0; deg < 360; deg += 7) {
			// north up the segment there, to the cap's circle
			const double lon = deg * M_PI / 180;
			Atlas::Node from = a.node(Atlas::Cell(Atlas::HOME, atlas.locate({ R * std::cos(lat) * std::cos(lon), R * std::sin(lat),
				R * std::cos(lat) * std::sin(lon) })));
			std::optional<Atlas::Step> step;
			for (int n = 0; n < 200 && !(step = a.step(from, 2, 1)); n++)
				from.z++;
			UASSERT(step.has_value());
			const Atlas::Cell segment = *a.cell(from), cap = *a.cell(step->node);
			if (cap.region != Kind::Cap)
				continue; // a step over the segment's own seam first
			entered++;
			const auto c = atlas.corners(segment), k = atlas.corners(cap);
			const V north = unit(sub(c[2], c[0]));
			const V capEast = unit(sub(k[1], k[0])), capNorth = unit(sub(k[2], k[0]));
			const std::array<int, 2> d = planet::Atlas::turned<int>(0, 1, step->turn);
			const V way = unit({ d[0] * capEast.x + d[1] * capNorth.x, d[0] * capEast.y + d[1] * capNorth.y,
				d[0] * capEast.z + d[1] * capNorth.z });
			const double agree = north.x * way.x + north.y * way.y + north.z * way.z;
			UTEST(agree > std::cos(50 * M_PI / 180), "at %d°, turn %d points %.1f° off north", deg,
					step->turn, std::acos(std::min(1.0, agree)) * 180 / M_PI);
			turned += step->turn != 0;
		}
		UASSERT(entered > 20);
		UASSERT(turned > 0); // the cap's grid isn't the segments' everywhere
	}
}

// The view of a region from an eye: the eye's own grid nearby is itself
// (to a hair: the chart's curvature), and a cell across any seam lands
// where the eye's grid carried on puts it, against the face it's across.
void TestPlanetAtlas::testViewIsTheGrid()
{
	for (const auto &p : planets()) {
		const Atlas a = Atlas::resident(p);
		size_t across = 0;
		for (const Atlas::Node n : edgeNodes(a, 4)) {
			const std::array<double, 3> at{ (double)n.x, (double)n.y, (double)n.z };
			// itself
			const Atlas::Frame eye = *a.frame(n, at);
			const Atlas::Affine self = Atlas::view(eye, eye);
			const auto same = self(n.x + 3, n.y - 2, n.z + 1);
			UTEST(std::abs(same[0] - (n.x + 3)) < 0.01 && std::abs(same[1] - (n.y - 2)) < 0.01 &&
					std::abs(same[2] - (n.z + 1)) < 0.01, "own grid moved by the view (%.3f)", same[0] - (n.x + 3));
			// a step across a seam: the far cell, seen, is a step from here
			for (const auto &[axis, sign] : STEPS) {
				const std::optional<Atlas::Step> s = a.step(n, axis, sign);
				if (!s)
					continue;
				across++;
				const std::array<double, 3> far{ (double)s->node.x, (double)s->node.y, (double)s->node.z };
				const auto seen = Atlas::view(eye, *a.frame(s->node, far))(far[0], far[1], far[2]);
				std::array<double, 3> want = at; // just past the face's middle: where step looks
				want[axis] += sign * 0.501;
				const double off = std::sqrt(std::pow(seen[0] - want[0], 2) + std::pow(seen[1] - want[1], 2) +
						std::pow(seen[2] - want[2], 2));
				// the far cell holds that point: its centre is within
				// half its diagonal of it (near the axis cells are wide), and the
				// view adds a hair
				const auto k = a.body(planet::Atlas::HOME).corners(*a.cell(s->node));
				const double half = 0.5 * std::sqrt(std::pow(k[7].x - k[0].x, 2) + std::pow(k[7].y - k[0].y, 2) +
						std::pow(k[7].z - k[0].z, 2));
				const Atlas::Cell from = *a.cell(n), to = *a.cell(s->node);
				const terragen::Atlas::Vec mid = a.body(planet::Atlas::HOME).centre(from);
				// at a cut (either side), the neighbour past the cut's edge or
				// nearest the face's middle; or where the point is a gap merged
				// into the cell (terragen::Atlas::locate): a little further
				const terragen::Atlas::Vec stepped = *a.point(n, want[0], want[1], want[2]);
				const bool cut = a.body(planet::Atlas::HOME).retained(*a.cell(n)) < 1 || a.body(planet::Atlas::HOME).retained(*a.cell(s->node)) < 1;
				const bool merged = !(a.body(planet::Atlas::HOME).owner(stepped) == *a.cell(s->node));
				const double slack = cut || merged ? 0.6 : 0.1;
				UTEST(off < half + slack, "a step across a seam is drawn %.3f away (cell half %.3f): region %d→%d, r %.1f, the step's point is %s the far cell",
						off, half, (int)from.region, (int)to.region, std::sqrt(mid.x * mid.x + mid.y * mid.y + mid.z * mid.z),
						a.body(planet::Atlas::HOME).locate(stepped) == to ? "in" : "NOT in");
			}
		}
		UTEST(across > 200, "only %zu crossings", across);
	}
}

/*
	The true cut (sprint 34): where grids meet, every point is in exactly one
	cell's solid (its node, or a cut cell's part on its side of the edge:
	Atlas::cut). No gap, no overlap. Points round cut cells, every grid's
	cell there counted; those within a hair of an edge (where the fitted
	plane and the true edge can disagree by its flatness) are left out.
*/
void TestPlanetAtlas::testCutsPartitionThePlanet()
{
	constexpr double HAIR = 0.01;
	for (const auto &p : planets()) {
		const Atlas a = Atlas::resident(p);
		const terragen::Atlas &t = a.body(planet::Atlas::HOME);
		std::mt19937_64 rng(34);
		std::uniform_real_distribution<double> unit(-0.5, 0.5);
		size_t points = 0, gaps = 0, overlaps = 0;
		// how many solids hold a point, over every grid
		const auto holders = [&](const terragen::Atlas::Vec &q) {
			int n = 0;
			for (const Atlas::Tile &r : a.tiles()) {
				const std::array<double, 3> g = t.grid(r.kind, r.index, q);
				const Atlas::Cell c(r.body, { r.kind, r.index, (int64_t)std::floor(g[0]), (int64_t)std::floor(g[1]),
					(int64_t)std::floor(g[2]) });
				const Atlas::Node node{ (int32_t)(r.rect.x + c.i - r.i0), (int32_t)c.k, (int32_t)(r.rect.z + c.j - r.j0) };
				const std::optional<Atlas::Cell> stored = a.cell(node);
				if (!stored || !(*stored == c))
					continue;
				const std::optional<Atlas::Cut> cut = a.cut(node);
				if (!cut || cut->at(g[0] - c.i - 0.5, g[2] - c.k - 0.5, g[1] - c.j - 0.5) >= 0)
					n++;
			}
			return n;
		};
		for (const Atlas::Node n : edgeNodes(a, 7)) {
			if (!a.cut(n))
				continue;
			const Atlas::Cell c = *a.cell(n);
			for (int k = 0; k < 32; k++) {
				const terragen::Atlas::Vec q = t.at(c, unit(rng), unit(rng), unit(rng));
				const double r = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z);
				if (r > p.radius + p.height - 1 || r < 1)
					continue;
				// a hair from any edge: the owner's territory, and this cell's
				if (std::abs(t.territory(c, q)) < HAIR || std::abs(t.territory(t.owner(q), q)) < HAIR)
					continue;
				points++;
				const int held = holders(q);
				gaps += held == 0;
				overlaps += held > 1;
			}
		}
		const double gap = (double)gaps / points, overlap = (double)overlaps / points;
		infostream << "cuts: radius " << p.radius << ": " << points << " points, " << gap * 100 << "% in no cell, "
				<< overlap * 100 << "% in two" << std::endl;
		UTEST(points > 1000, "only %zu points by cuts", points);
		UTEST(gap < 1e-3 && overlap < 1e-3, "radius %.0f: %.3f%% of points in no cell, %.3f%% in two", p.radius,
				gap * 100, overlap * 100);
	}
}

/*
	The window (planning/sprint-39-tiles.md), on a planet far too big to hold
	at once: tiles go in and out of slots, and wherever a tile is, its nodes
	are the same cells at the same spots.
*/
namespace {

const terragen::Atlas::Params BIG = terragen::Atlas::standard(250000);

// A place on the equator in a segment's middle (97.5° east: segment 6's).
terragen::Atlas::Vec middle()
{
	const double lon = 97.5 * M_PI / 180, r = BIG.radius - 1;
	return { r * std::cos(lon), 0, r * std::sin(lon) };
}

// A slot's node at (dx, y, dz) from its corner.
Atlas::Node inSlot(int slot, int32_t dx, int32_t y, int32_t dz)
{
	return { Atlas::ORIGIN + (slot % Atlas::SLOTS) * Atlas::SLOT + dx, y,
		Atlas::ORIGIN + (slot / Atlas::SLOTS) * Atlas::SLOT + dz };
}

} // namespace

void TestPlanetAtlas::testTileMovesWhole()
{
	Atlas a(BIG);
	UASSERT(a.tiles().empty());
	const Atlas::Cell somewhere(Atlas::HOME, a.body(planet::Atlas::HOME).locate(middle()));
	const Atlas::TileId id = a.tileOf(somewhere);
	a.put(3, id);
	std::vector<std::pair<Atlas::Cell, Atlas::Spot>> seen;
	for (int32_t d : { 0, 17, 1000, 4095 }) {
		const std::optional<Atlas::Cell> c = a.cell(inSlot(3, d, -5, 4095 - d));
		UASSERT(c);
		seen.push_back({ *c, *a.spot(inSlot(3, d, -5, 4095 - d)) });
		UASSERT(a.tileOf(*c) == id);
	}
	const uint64_t was = a.generation();
	a.put(3, std::nullopt);
	UASSERT(!a.cell(inSlot(3, 17, -5, 4078)));
	a.put(200, id);
	UASSERT(a.generation() == was + 2);
	UASSERT(*a.slotOf(id) == 200);
	size_t n = 0;
	for (int32_t d : { 0, 17, 1000, 4095 }) {
		const std::optional<Atlas::Cell> c = a.cell(inSlot(200, d, -5, 4095 - d));
		UASSERT(c && *c == seen[n].first);
		const Atlas::Spot s = *a.spot(inSlot(200, d, -5, 4095 - d));
		UASSERT(s.x == seen[n].second.x && s.y == seen[n].second.y && s.z == seen[n].second.z);
		UASSERT(a.node(*c).x == inSlot(200, d, -5, 4095 - d).x);
		n++;
	}
	// a cell's spot is its grid's place, whatever holds it
	UASSERT(a.spotOf(seen[0].first).x == seen[0].second.x);
	// one tile, one slot
	EXCEPTION_CHECK(std::invalid_argument, a.put(4, id));
	// a tile off its grid isn't one
	EXCEPTION_CHECK(std::invalid_argument, a.put(4, Atlas::TileId{ 0, 0, 1000000, 0 }));
	// a cell whose tile is out of the window has no node
	a.put(200, std::nullopt);
	EXCEPTION_CHECK(std::invalid_argument, a.node(seen[0].first));
}

void TestPlanetAtlas::testWindowCode()
{
	Atlas a(BIG);
	a.put(0, Atlas::TileId{ 0, 0, 0, 0 });
	a.put(57, Atlas::TileId{ 0, 25, -3, 2 });
	a.put(224, Atlas::TileId{ 0, 7, -1, -40 });
	Atlas b(BIG);
	b.put(9, Atlas::TileId{ 0, 1, 0, 0 });
	b.setWindow(a.windowCode());
	UASSERT(b.window() == a.window());
	UASSERT(b.windowCode() == a.windowCode());
	EXCEPTION_CHECK(std::invalid_argument, b.setWindow("tiles-v2 3:x"));
	EXCEPTION_CHECK(std::invalid_argument, b.setWindow("tiles-v0"));
}

// Two neighbouring tiles of a grid in slots far apart: a step off one's
// edge lands on the other's next cell.
void TestPlanetAtlas::testStepsBetweenSlots()
{
	Atlas a(BIG);
	const Atlas::Cell c(Atlas::HOME, a.body(planet::Atlas::HOME).locate(middle()));
	const Atlas::TileId t = a.tileOf(c);
	const Atlas::TileId east{ t.body, t.region, t.ti + 1, t.tj };
	a.put(10, t);
	a.put(140, east);
	// the last column of t, at the ground
	const Atlas::Cell last(c.body, { c.region, c.index, (int64_t)t.ti * Atlas::TILE + Atlas::TILE - 1, c.j, -2 });
	const Atlas::Node from = a.node(last);
	const std::optional<Atlas::Step> s = a.step(from, 0, 1);
	UASSERT(s);
	UASSERT(s->turn == 0);
	const Atlas::Cell next = *a.cell(s->node);
	UASSERT(next == Atlas::Cell(c.body, { c.region, c.index, last.i + 1, last.j, last.k }));
	UASSERT(*Atlas::slotAt(s->node) == 140);
	// and back
	const std::optional<Atlas::Step> back = a.step(s->node, 0, -1);
	UASSERT(back && back->node.x == from.x && back->node.z == from.z);
}

void TestPlanetAtlas::testTilesNear()
{
	const Atlas a(BIG);
	const Atlas::Cell c(Atlas::HOME, a.body(planet::Atlas::HOME).locate(middle()));
	const Atlas::TileId t = a.tileOf(c);
	// at a corner of its tile, a ball reaches the three round it too
	const Atlas::Cell corner(c.body, { c.region, c.index, (int64_t)t.ti * Atlas::TILE, (int64_t)t.tj * Atlas::TILE, 0 });
	const std::vector<Atlas::TileId> near = a.tilesNear(Atlas::HOME, a.body(planet::Atlas::HOME).centre(corner), 600);
	UASSERT(std::find(near.begin(), near.end(), t) != near.end());
	for (const Atlas::TileId &o : { Atlas::TileId{ t.body, t.region, t.ti - 1, t.tj }, Atlas::TileId{ t.body, t.region, t.ti, t.tj - 1 },
				Atlas::TileId{ t.body, t.region, t.ti - 1, t.tj - 1 } })
		UASSERT(std::find(near.begin(), near.end(), o) != near.end());
	UASSERT(near.size() == 4);
	// a cap's edge reaches its segments
	const Atlas::Cell pole(Atlas::HOME, a.body(planet::Atlas::HOME).locate({ 0, BIG.radius - 1, 0 }));
	UASSERT(pole.region == terragen::Atlas::Region::Cap);
}
