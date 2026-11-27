// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tests/unit/test.h"

#include "src/server/world/run/ground/ground.h"
#include "src/server/world/run/machines/workshop/motion.h"
#include "src/shared/game/objects/collision.h"
#include "src/shared/base/dummygamedef.h"
#include "src/shared/map/dummymap.h"
#include "src/shared/game/environment/environment.h"
#include "src/shared/game/objects/object_properties.h"
#include "src/shared/platform/network/networkprotocol.h"
#include "src/shared/base/noise.h"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <map>
#include <memory>

using namespace ground;

class TestGround : public TestBase {
public:
	TestGround() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestGround"; }

	void runTests(IGameDef *gamedef);

	void testFlatStaysFlat();
	void testAStepIsARamp();
	void testTwoBlocksIsACliff();
	void testSolidKeepsItsShape();
	void testNeighboursAgree();
	void testTheClientKnowsLoose();
	void testUnknownAndCubesAreWalls();
	void testWalksUpASteppedHill();
	void testFirstCutKeepsTheShape();
	void testBlockBoxesAsTheMapSays();
	void testRisesStandTheLandFine();
	void testNaturalGroundIsOneField();
	void testNaturalRockFollowsTheField();
};

static TestGround g_test_instance;

void TestGround::runTests(IGameDef *gamedef)
{
	TEST(testFlatStaysFlat);
	TEST(testAStepIsARamp);
	TEST(testTwoBlocksIsACliff);
	TEST(testSolidKeepsItsShape);
	TEST(testNeighboursAgree);
	TEST(testTheClientKnowsLoose);
	TEST(testUnknownAndCubesAreWalls);
	TEST(testWalksUpASteppedHill);
	TEST(testFirstCutKeepsTheShape);
	TEST(testBlockBoxesAsTheMapSays);
	TEST(testRisesStandTheLandFine);
	TEST(testNaturalGroundIsOneField);
	TEST(testNaturalRockFollowsTheField);
}

namespace {

// Columns of ground: each a surface level and what it's made of; air above.
struct Land
{
	std::unique_ptr<NodeDefManager> ndef{ createNodeDefManager() };
	content_t dirt, stone;
	std::map<std::pair<int, int>, std::pair<int, bool>> columns; // (x, z) → (top, solid)
	int rest = 0; // the surface beyond the columns set
	std::map<std::pair<int, int>, int> rises; // (x, z) → its top block's rise (ground::Rises)

	Land()
	{
		ContentFeatures d;
		d.name = "test:dirt";
		d.walkable = true;
		d.groups["loose"] = 1;
		dirt = ndef->set(d.name, std::move(d));
		ContentFeatures s;
		s.name = "test:stone";
		s.walkable = true;
		stone = ndef->set(s.name, std::move(s));
	}

	void set(int x, int z, int top, bool solid = false) { columns[{ x, z }] = { top, solid }; }

	MapNode operator()(v3s16 p) const
	{
		auto it = columns.find({ p.X, p.Z });
		int top = it == columns.end() ? rest : it->second.first;
		bool solid = it != columns.end() && it->second.second;
		if (p.Y >= top)
			return MapNode(CONTENT_AIR);
		return MapNode(solid ? stone : dirt);
	}

	// the smoothed top of the column at (x, z)
	std::optional<Top> top(int x, int z) const
	{
		auto it = columns.find({ x, z });
		int t = it == columns.end() ? rest : it->second.first;
		const auto rise = [&](v3s16 p) {
			auto r = rises.find({ p.X, p.Z });
			auto c = columns.find({ p.X, p.Z });
			const int top = c == columns.end() ? rest : c->second.first;
			return r != rises.end() && p.Y == top - 1 ? std::optional<int>(r->second) : std::nullopt;
		};
		return Ground(*ndef).top(*this, [](v3s16) { return false; }, rise, v3s16(x, t - 1, z));
	}

	// any loose block's shape (a slice of natural ground under its top too)
	std::optional<Top> at(int x, int y, int z) const
	{
		const auto rise = [&](v3s16 p) {
			auto r = rises.find({ p.X, p.Z });
			auto c = columns.find({ p.X, p.Z });
			const int top = c == columns.end() ? rest : c->second.first;
			return r != rises.end() && p.Y == top - 1 ? std::optional<int>(r->second) : std::nullopt;
		};
		return Ground(*ndef).top(*this, [](v3s16) { return false; }, rise, v3s16(x, y, z));
	}
};

} // namespace

void TestGround::testFlatStaysFlat()
{
	Land land;
	auto t = land.top(0, 0);
	UASSERT(t);
	for (int i = 0; i < 4; i++) {
		UASSERTEQ(int, t->corners[i], STEPS);
		UASSERTEQ(int, t->sides[i][0], STEPS);
		UASSERTEQ(int, t->sides[i][1], STEPS);
	}
	// stood on as one box, the whole block
	std::vector<aabb3f> boxes;
	t->boxes(boxes);
	UASSERTEQ(size_t, boxes.size(), 1);
	UASSERT(boxes[0].MinEdge.equals(v3f(-0.5f * BS)) && boxes[0].MaxEdge.equals(v3f(0.5f * BS)));
	// not a top: ground over it
	UASSERT(!Ground(*land.ndef).top(land, v3s16(0, -2, 0)));
}

void TestGround::testAStepIsARamp()
{
	// a step up of one block at x = 1, along z
	Land land;
	for (int x = 1; x <= 8; x++)
		for (int z = -8; z <= 8; z++)
			land.set(x, z, 1);
	auto low = land.top(0, 0), high = land.top(1, 0);
	UASSERT(low && high);
	// the step's edge, at x = 1: halfway up, seen from both sides
	UASSERTEQ(int, low->corners[1], STEPS + STEPS / 2);
	UASSERTEQ(int, low->corners[3], STEPS + STEPS / 2);
	UASSERTEQ(int, high->corners[0], STEPS / 2);
	UASSERTEQ(int, high->corners[2], STEPS / 2);
	// the far edges as they were
	UASSERTEQ(int, low->corners[0], STEPS);
	UASSERTEQ(int, high->corners[1], STEPS);
	// the surface is one ramp: no wall either side
	UASSERTEQ(int, high->sides[0][0], high->corners[0]);
	UASSERTEQ(int, low->sides[1][1], low->corners[3]);
	// halfway across the high block, three quarters up
	UASSERT(std::fabs(high->height(0.5f, 0.5f) - 0.75f) < 1e-4f);
}

void TestGround::testTwoBlocksIsACliff()
{
	Land land;
	for (int x = 1; x <= 8; x++)
		for (int z = -8; z <= 8; z++)
			land.set(x, z, 2);
	auto low = land.top(0, 0), high = land.top(1, 0);
	UASSERT(low && high);
	// neither leans toward the other
	UASSERTEQ(int, low->corners[1], STEPS);
	UASSERTEQ(int, high->corners[0], STEPS);
	// the high side's wall reaches down to (into) the low side's ground
	UASSERT(high->sides[0][0] <= -STEPS);
}

void TestGround::testSolidKeepsItsShape()
{
	// stone at the same height beside dirt, lower ground the other way
	Land land;
	land.set(1, 0, 0, true);
	land.set(-1, 0, -1);
	land.set(-1, 1, -1);
	land.set(-1, -1, -1);
	land.set(0, -1, -1);
	land.set(1, -1, -1);
	UASSERT(!land.top(1, 0)); // stone isn't smoothed
	auto t = land.top(0, 0);
	UASSERT(t);
	// beside the stone the dirt stays full, though the corner's ground
	// averages lower: the stone's face against it stays hidden
	UASSERTEQ(int, t->corners[1], STEPS);
	UASSERTEQ(int, t->corners[3], STEPS);
	// toward the low side it leans down
	UASSERT(t->corners[0] < STEPS);
}

/*
	A rough field of dirt with some stone: for every smoothed column, what
	it says of each neighbour's edge is what that neighbour draws there,
	where it's within a block (so the surface has no cracks between them);
	and it never falls under its own surface beside anything that would
	have hidden a face against it.
*/
void TestGround::testNeighboursAgree()
{
	Land land;
	const int N = 24;
	for (int x = -N; x <= N; x++)
		for (int z = -N; z <= N; z++) {
			float n = noise2d_value(x / 5.f, z / 5.f, 7) + 0.3f * noise2d(x, z, 11);
			int top = int(std::floor(n * 3));
			land.set(x, z, top, noise2d(x * 5, z * 5, 13) > 0.7f);
		}
	int checked = 0, lowered = 0, raised = 0;
	for (int x = -N + 4; x <= N - 4; x++)
		for (int z = -N + 4; z <= N - 4; z++) {
			auto a = land.top(x, z);
			if (!a)
				continue;
			int ta = land.columns[{ x, z }].first;
			for (int i = 0; i < 4; i++) {
				lowered += a->corners[i] < STEPS;
				raised += a->corners[i] > STEPS;
			}
			for (int s = 0; s < 4; s++) {
				int nx = x + SIDE_DIR[s][0], nz = z + SIDE_DIR[s][1];
				auto [tb, solid] = land.columns[{ nx, nz }];
				for (int k = 0; k < 2; k++) {
					int i = SIDE_CORNERS[s][k];
					int mine = a->corners[i] + (ta - 1) * STEPS;
					int theirs = a->sides[s][k] + (ta - 1) * STEPS;
					if (tb > ta || (tb == ta && solid))
						UASSERT(mine >= ta * STEPS);
					if (solid) {
						UASSERTEQ(int, theirs, tb * STEPS);
						continue;
					}
					if (std::abs(tb - ta) >= 2)
						continue;
					auto b = land.top(nx, nz);
					UASSERT(b);
					// their corner at this same world corner
					int j = (i & 1) - SIDE_DIR[s][0] + 2 * ((i >> 1) - SIDE_DIR[s][1]);
					UASSERTEQ(int, theirs, b->corners[j] + (tb - 1) * STEPS);
					checked++;
				}
			}
		}
	// the field was rough enough to mean something
	UASSERT(checked > 1000);
	UASSERT(lowered > 100);
	UASSERT(raised > 100);
}

// The client's definitions come from the server's, sent: loose with them
// (it's derived, not sent).
void TestGround::testTheClientKnowsLoose()
{
	Land land;
	std::ostringstream os(std::ios::binary);
	land.ndef->serialize(os, LATEST_PROTOCOL_VERSION);
	std::unique_ptr<NodeDefManager> client(createNodeDefManager());
	std::istringstream is(os.str(), std::ios::binary);
	client->deSerialize(is, LATEST_PROTOCOL_VERSION);
	UASSERT(client->get(land.dirt).loose);
	UASSERT(!client->get(land.stone).loose);
	UASSERT(Ground(*client).kind(MapNode(land.dirt)) == Kind::Loose);
}

namespace {

class HillEnvironment : public Environment {
	DummyMap map;
public:
	HillEnvironment(IGameDef *gamedef) : Environment(gamedef), map(gamedef, {-1, -1, -1}, {1, 1, 1}) {}
	void step(f32 dtime) override {}
	Map &getMap() override { return map; }
	void getSelectedActiveObjects(const core::line3d<f32> &, std::vector<PointedThing> &,
			const std::optional<Pointabilities> &) override {}
};

} // namespace

/*
	A player-sized mover walks up a hill of dirt that rises a whole block
	every three: a cube's step is more than it can step up (0.6), smoothed
	ground's voxels aren't. Through the collision the player moves by, the
	whole way never inside the ground. With the ground stood on as cubes it
	stops at the first step.
*/
void TestGround::testWalksUpASteppedHill()
{
	DummyGameDef gamedef;
	NodeDefManager *ndef = gamedef.getWritableNodeDefManager();
	ContentFeatures d;
	d.name = "test:dirt";
	d.walkable = true;
	d.groups["loose"] = 1;
	const content_t dirt = ndef->set(d.name, std::move(d));

	HillEnvironment env(&gamedef);
	Map &map = env.getMap();
	auto top = [](int x) { return 2 + int(std::floor(x / 3.f)); };
	for (s16 x = -16; x < 32; x++)
		for (s16 z = -16; z < 32; z++)
			for (s16 y = -16; y < 32; y++)
				map.setNode(v3s16(x, y, z), MapNode(y < top(x) ? dirt : CONTENT_AIR));

	const aabb3f box(-0.3f * BS, 0, -0.3f * BS, 0.3f * BS, 1.77f * BS, 0.3f * BS);
	v3f pos(-10 * BS, (top(-10) + 0.5f) * BS, 0);
	v3f speed;
	bool touching = false;
	for (int i = 0; i < 400 && pos.X < 24 * BS; i++) {
		speed.X = 4 * BS;
		speed.Z = 0;
		const f32 stepheight = touching ? 0.6f * BS : 0.2f * BS; // as the local player's
		CollisionMoveResult r = collisionMoveSimple(&env, &gamedef, box, stepheight, 0.05f,
				&pos, &speed, v3f(0, -9.81f * BS, 0), nullptr, false, StepUpMode::LEGACY);
		touching = r.touching_ground;
		UASSERT(!collision_check_intersection(&env, &gamedef, box, pos, nullptr, false));
	}
	UTEST(pos.X >= 24 * BS, "stopped at x %.2f", pos.X / BS);
	// and stands on it there: its feet at the voxels under its middle, or a
	// little above (its box is wider than a voxel)
	const int x = int(std::floor(pos.X / BS + 0.5f));
	auto t = Ground(*ndef).top([&](v3s16 p) { return map.getNode(p); }, v3s16(x, top(x) - 1, 0));
	UASSERT(t);
	const float under = top(x) - 1.5f + t->height(pos.X / BS + 0.5f - x, 0.5f);
	const float feet = pos.Y / BS;
	UTEST(feet >= under - 1e-3f && feet < under + 0.3f, "feet %.3f, ground %.3f", feet, under);
}

/*
	Beside a column not all known (its block not here yet) or a cell that
	keeps its cube (an atlas's cut cell), the ground isn't lowered: either
	may be a full block whose face against it is hidden. Ground falling away
	on the far side, so the corners there would otherwise lean down.
*/
void TestGround::testUnknownAndCubesAreWalls()
{
	Land land;
	for (int z = -1; z <= 1; z++) {
		land.set(-1, z, -1);
		land.set(0, z, 0);
		land.set(1, z, 0);
	}
	land.set(0, -1, -1);
	land.set(1, -1, -1);
	const Ground ground(*land.ndef);
	// as it is: the corner at (1, 0) leans down toward the low row at z = −1
	auto open = ground.top(land, v3s16(0, -1, 0));
	UASSERT(open && open->corners[1] < STEPS);

	// the column at (1, 0) unknown
	auto unknown = [&](v3s16 p) {
		return (p.X == 1 && p.Z == 0) ? MapNode(CONTENT_IGNORE) : land(p);
	};
	auto t = ground.top(unknown, v3s16(0, -1, 0));
	UASSERT(t);
	UASSERTEQ(int, t->corners[1], STEPS);
	UASSERTEQ(int, t->corners[3], STEPS);

	// the column at (1, 0) a cube (cut)
	auto cube = [](v3s16 p) { return p.X == 1 && p.Z == 0; };
	t = ground.top(land, cube, [](v3s16) { return std::optional<int>(); }, v3s16(0, -1, 0));
	UASSERT(t);
	UASSERTEQ(int, t->corners[1], STEPS);
	UASSERTEQ(int, t->corners[3], STEPS);
	// and a cube itself isn't smoothed
	UASSERT(!ground.top(land, cube, [](v3s16) { return std::optional<int>(); }, v3s16(1, -1, 0)));
}

/*
	Smoothed ground cut the first time (Map::settleGround) keeps the shape
	it had, its own cell and what rose into the one above: what's stood on
	there is the same; and the ground round it reads it as before, so its
	shape doesn't change either.
*/
void TestGround::testFirstCutKeepsTheShape()
{
	DummyGameDef gamedef;
	NodeDefManager *ndef = gamedef.getWritableNodeDefManager();
	ContentFeatures d;
	d.name = "test:dirt";
	d.walkable = true;
	d.drawtype = NDT_NORMAL;
	d.groups["loose"] = 1;
	const content_t dirt = ndef->set(d.name, std::move(d));

	HillEnvironment env(&gamedef);
	Map &map = env.getMap();
	auto top = [](int x, int z) { return 4 + int(std::floor((x + z / 2) / 3.f)); };
	for (s16 x = -16; x < 32; x++)
		for (s16 z = -16; z < 32; z++)
			for (s16 y = -16; y < 32; y++)
				map.setNode(v3s16(x, y, z), MapNode(y < top(x, z) ? dirt : CONTENT_AIR));

	// what's stood on in a column of cells, as sixteenth columns (x, z) → height
	auto stood = [&](v3s16 p) {
		std::map<std::pair<int, int>, int> h;
		for (s16 dy = 0; dy < 2; dy++) {
			std::vector<aabb3f> boxes;
			const v3s16 q = p + v3s16(0, dy, 0);
			if (!map.getShapedBoxes(q, boxes) && map.getNode(q).getContent() == dirt)
				boxes.emplace_back(-0.5f * BS, -0.5f * BS, -0.5f * BS, 0.5f * BS, 0.5f * BS, 0.5f * BS);
			for (const aabb3f &b : boxes)
				for (int x = 0; x < STEPS; x++)
					for (int z = 0; z < STEPS; z++) {
						const v3f mid((x + 0.5f) / STEPS - 0.5f, 0, (z + 0.5f) / STEPS - 0.5f);
						if (mid.X * BS > b.MinEdge.X && mid.X * BS < b.MaxEdge.X
								&& mid.Z * BS > b.MinEdge.Z && mid.Z * BS < b.MaxEdge.Z) {
							int &v = h[{ x, z }];
							v = std::max(v, dy * STEPS + int(std::lround((b.MaxEdge.Y / BS + 0.5f) * STEPS)));
						}
					}
		}
		return h;
	};

	int risen = 0;
	for (s16 x = 2; x < 12; x++) {
		const v3s16 p(x, top(x, 0) - 1, 0);
		const std::optional<Top> was = map.groundTop(p);
		UASSERT(was);
		risen += !was->shape(1).empty();
		const auto before = stood(p);
		std::map<v3s16, std::optional<Top>> around;
		for (s16 dx = -2; dx <= 2; dx++)
			for (s16 dz = -2; dz <= 2; dz++)
				if (dx || dz) {
					const v3s16 q(x + dx, top(x + dx, dz) - 1, dz);
					around[q] = map.groundTop(q);
				}

		map.settleGround(p);
		// nothing's left to the rule that the chisel would start from as a
		// cube: smoothed still only where it was a cube and nothing rose
		if (const std::optional<Top> still = map.groundTop(p))
			UASSERT(still->shape(0).isWhole() && still->shape(1).empty());
		UASSERT(stood(p) == before);
		for (const auto &[q, t] : around) {
			const std::optional<Top> now = map.groundTop(q);
			UASSERT(now.has_value() == t.has_value());
			if (t)
				UASSERT(now->cells == t->cells);
		}
	}
	UASSERT(risen > 0); // some rose into the cell above
}

/*
	A block's ground as physics stands on it (workshop::groundBoxes, which
	reads each node once for the smoothing it does) is what the map says,
	node by node: Map::getShapedBoxes, or a node's own boxes. A rough
	hill of loose ground with stone in it, across blocks.
*/
void TestGround::testBlockBoxesAsTheMapSays()
{
	DummyGameDef gamedef;
	NodeDefManager *ndef = gamedef.getWritableNodeDefManager();
	ContentFeatures d;
	d.name = "test:dirt";
	d.walkable = true;
	d.groups["loose"] = 1;
	const content_t dirt = ndef->set(d.name, std::move(d));
	ContentFeatures st;
	st.name = "test:stone";
	st.walkable = true;
	const content_t stone = ndef->set(st.name, std::move(st));

	HillEnvironment env(&gamedef);
	Map &map = env.getMap();
	auto top = [](int x, int z) { return 4 + int(std::floor(3 * std::sin(x * 0.7) + 2 * std::cos(z * 0.45) + ((x * 7 + z * 13) % 5 == 0 ? 2 : 0))); };
	for (s16 x = -16; x < 32; x++)
		for (s16 z = -16; z < 32; z++)
			for (s16 y = -16; y < 32; y++)
				map.setNode(v3s16(x, y, z), MapNode(y >= top(x, z) ? CONTENT_AIR
						: (x * 3 + z) % 11 == 0 ? stone : dirt));

	for (const v3s16 block : { v3s16(0, 0, 0), v3s16(0, -1, 0), v3s16(1, 0, 1) }) {
		std::vector<aabb3f> got;
		const auto t0 = std::chrono::steady_clock::now();
		UASSERT(workshop::groundBoxes(map, ndef, block, got));
		const auto t1 = std::chrono::steady_clock::now();
		// as it was read before: through the map, a node at a time
		std::vector<aabb3f> want, nb;
		const v3s16 base = block * MAP_BLOCKSIZE;
		auto whole = [&](v3s16 p) {
			const ContentFeatures &f = ndef->get(map.getNode(p));
			bool is = f.walkable && f.drawtype == NDT_NORMAL && !map.getShape(p);
			if (is && f.loose) {
				const v3s16 up = p + v3s16(0, 1, 0);
				const ContentFeatures &over = ndef->get(map.getNode(up));
				if (!(over.walkable && !map.getShape(up))) {
					std::vector<aabb3f> shaped;
					is = !map.getShapedBoxes(p, shaped);
				}
			}
			return is;
		};
		for (s16 x = 0; x < MAP_BLOCKSIZE; x++)
			for (s16 y = 0; y < MAP_BLOCKSIZE; y++)
				for (s16 z = 0; z < MAP_BLOCKSIZE; z++) {
					const v3s16 rel(x, y, z), p = base + rel;
					const MapNode n = map.getNode(p);
					if (!ndef->get(n).walkable)
						continue;
					const bool inner = x > 0 && y > 0 && z > 0 && x < MAP_BLOCKSIZE - 1 && y < MAP_BLOCKSIZE - 1
							&& z < MAP_BLOCKSIZE - 1;
					bool buried = inner && whole(p);
					for (const v3s16 &dir : { v3s16(1, 0, 0), v3s16(-1, 0, 0), v3s16(0, 1, 0), v3s16(0, -1, 0),
							v3s16(0, 0, 1), v3s16(0, 0, -1) })
						buried = buried && whole(p + dir);
					if (buried)
						continue;
					nb.clear();
					if (whole(p))
						nb.push_back(aabb3f(v3f(-0.5f * BS), v3f(0.5f * BS)));
					else if (!map.getShapedBoxes(p, nb))
						n.getCollisionBoxes(ndef, &nb, n.getNeighbors(p, &map));
					for (aabb3f b : nb) {
						b.MinEdge = b.MinEdge / BS + v3f::from(rel);
						b.MaxEdge = b.MaxEdge / BS + v3f::from(rel);
						if (b.getVolume() > 0)
							want.push_back(b);
					}
				}
		const auto t2 = std::chrono::steady_clock::now();
		auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
		rawstream << "ground boxes (" << block.X << ", " << block.Y << ", " << block.Z << "): " << got.size()
				<< " boxes in " << ms(t0, t1) << " ms, node by node " << ms(t1, t2) << " ms" << std::endl;
		UTEST(got.size() == want.size(), "block (%d, %d, %d): %zu boxes, the map's %zu",
				block.X, block.Y, block.Z, got.size(), want.size());
		UASSERT(want.size() > 0);
		for (size_t i = 0; i < want.size(); i++)
			UASSERT(got[i].MinEdge.equals(want[i].MinEdge) && got[i].MaxEdge.equals(want[i].MaxEdge));
	}
}

// Sprint 128: the world model stands its land to a sixteenth (ground::Rises).
// A field all risen stands that far over its faces; a slope of rises is a
// slope, every corner agreed by the columns round it; and a rise never
// carries ground over a cliff, nor moves a solid.
void TestGround::testRisesStandTheLandFine()
{
	{
		Land land;
		for (int x = -4; x <= 4; x++)
			for (int z = -4; z <= 4; z++)
				land.rises[{ x, z }] = -5;
		auto t = land.top(0, 0);
		UASSERT(t);
		for (int i = 0; i < 4; i++)
			UASSERTEQ(int, t->corners[i], STEPS - 5);
	}
	{
		// a gentle slope up x: a sixteenth more a column, all on one level
		Land land;
		for (int x = -6; x <= 6; x++)
			for (int z = -4; z <= 4; z++)
				land.rises[{ x, z }] = std::clamp(x, -8, 7);
		for (int x = -3; x <= 3; x++) {
			auto t = land.top(x, 0), next = land.top(x + 1, 0);
			UASSERT(t && next);
			// (the corner between columns x and x + 1: their rises' average, to the nearest)
			UASSERTEQ(int, t->corners[1], next->corners[0]);
			UASSERTEQ(int, t->corners[3], next->corners[2]);
			UASSERT(t->corners[1] > t->corners[0]); // it rises
			UASSERT(std::abs(t->corners[1] - (STEPS + x)) <= 1);
		}
	}
	{
		// two blocks up is still a cliff: the high side's rise stays its own
		Land land;
		for (int x = 1; x <= 8; x++)
			for (int z = -8; z <= 8; z++) {
				land.set(x, z, 2);
				land.rises[{ x, z }] = 6;
			}
		auto low = land.top(0, 0);
		UASSERT(low);
		UASSERTEQ(int, low->corners[1], STEPS);
		UASSERTEQ(int, low->corners[3], STEPS);
	}
	{
		// a solid with a rise is natural rock: shaped (sprint 129); without, it keeps its block
		Land land;
		land.set(1, 0, 1, true);
		UASSERT(!land.top(1, 0));
		land.rises[{ 1, 0 }] = 7;
		UASSERT(land.top(1, 0));
	}
}

// Sprint 129: natural ground is one field. A lone column a block and a half
// over its neighbours (a crest the old clusters left a box) comes down: its
// corners are the four columns' average, under its own top block, so that
// block is empty and the one under it a slice, the surface in it; every
// corner agrees with its neighbours'.
void TestGround::testNaturalGroundIsOneField()
{
	Land land;
	for (int x = -4; x <= 4; x++)
		for (int z = -4; z <= 4; z++)
			land.rises[{ x, z }] = 0;
	land.set(0, 0, 2);
	land.rises[{ 0, 0 }] = 0;
	// (the spike's top block, y 1: its corners the average of 32 and three 0s)
	auto top = land.top(0, 0);
	UASSERT(top);
	for (int i = 0; i < 4; i++)
		UASSERTEQ(int, top->corners[i], 8 - STEPS);
	for (int x = 0; x < STEPS; x++)
		for (int z = 0; z < STEPS; z++) {
			UASSERTEQ(int, top->cells[x][z], 0);
			UASSERT(!top->surface[x][z]);
		}
	// the block under it: a slice, its surface half way up
	auto slice = land.at(0, 0, 0);
	UASSERT(slice);
	UASSERTEQ(int, slice->below, 1);
	for (int x = 0; x < STEPS; x++)
		for (int z = 0; z < STEPS; z++) {
			UASSERTEQ(int, slice->cells[x][z], 8);
			UASSERT(slice->surface[x][z]);
		}
	// its neighbour's corner there is the same place (its top block is y −1)
	auto beside = land.top(1, 0);
	UASSERT(beside);
	UASSERTEQ(int, beside->corners[0], 8 + STEPS);
	// a block the field stands over is whole: no slice
	UASSERT(!land.at(1, -2, 0));
	// and under ground that isn't natural, no slice either
	Land dug;
	dug.set(0, 0, 2);
	UASSERT(!dug.at(0, 0, 0));
}

// Natural rock is the field's too: a spike of rock among natural ground comes
// down as loose ground does, cut through its own top and the rock under it.
// The same rock built (no rise) keeps its block.
void TestGround::testNaturalRockFollowsTheField()
{
	Land land;
	for (int x = -4; x <= 4; x++)
		for (int z = -4; z <= 4; z++)
			land.rises[{ x, z }] = 0;
	land.set(0, 0, 2, true);
	auto top = land.top(0, 0);
	UASSERT(top);
	for (int i = 0; i < 4; i++)
		UASSERTEQ(int, top->corners[i], 8 - STEPS);
	auto slice = land.at(0, 0, 0);
	UASSERT(slice);
	UASSERTEQ(int, slice->cells[8][8], 8);

	land.rises.erase({ 0, 0 });
	UASSERT(!land.top(0, 0));
	UASSERT(!land.at(0, 0, 0));
}
