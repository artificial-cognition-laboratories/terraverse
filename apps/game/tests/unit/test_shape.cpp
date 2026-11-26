// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tests/unit/test.h"

#include "library/geometry/shape/shape.h"
#include "src/shared/map/dummymap.h"
#include "src/shared/map/block/mapblock.h"
#include "src/shared/base/noise.h"
#include "src/shared/base/serialization.h"
#include <sstream>

using namespace shape;

class TestShape : public TestBase {
public:
	TestShape() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestShape"; }

	void runTests(IGameDef *gamedef);

	void testBoxesAreTheShape();
	void testKeptOnDiskAndSent(IGameDef *gamedef);
	void testGoesWithItsMaterial(IGameDef *gamedef);
	void testAim();
};

static TestShape g_test_instance;

void TestShape::runTests(IGameDef *gamedef)
{
	TEST(testBoxesAreTheShape);
	TEST(testKeptOnDiskAndSent, gamedef);
	TEST(testGoesWithItsMaterial, gamedef);
	TEST(testAim);
}

namespace {

// A rough shape: about half its sixteenths, in lumps.
Shape rough(int seed)
{
	Shape s = Shape::whole();
	for (s16 x = 0; x < STEPS; x++)
		for (s16 y = 0; y < STEPS; y++)
			for (s16 z = 0; z < STEPS; z++)
				if (noise3d_value(x / 3.f, y / 3.f, z / 3.f, seed) > 0)
					s.set({ x, y, z }, false);
	return s;
}

} // namespace

// Every sixteenth that's filled is in exactly one box; none that's empty is.
void TestShape::testBoxesAreTheShape()
{
	const Shape s = rough(3);
	UASSERT(!s.empty() && !s.isWhole());
	std::vector<aabb3f> boxes;
	s.boxes(boxes);
	for (s16 x = 0; x < STEPS; x++)
		for (s16 y = 0; y < STEPS; y++)
			for (s16 z = 0; z < STEPS; z++) {
				const v3f mid = box({ x, y, z }).getCenter();
				int in = 0;
				for (const aabb3f &b : boxes)
					in += b.isPointInside(mid);
				UASSERTEQ(int, in, s.has({ x, y, z }) ? 1 : 0);
			}
	// merged, not one a sixteenth
	UASSERT(boxes.size() < 1000);
}

// A block's cut nodes survive the disk and the network; a block from
// before (format 29) has none.
void TestShape::testKeptOnDiskAndSent(IGameDef *gamedef)
{
	const Shape s = rough(5);
	for (bool disk : { true, false }) {
		MapBlock block({}, gamedef);
		for (s16 x = 0; x < MAP_BLOCKSIZE; x++)
			for (s16 y = 0; y < MAP_BLOCKSIZE; y++)
				for (s16 z = 0; z < MAP_BLOCKSIZE; z++)
					block.setNodeNoCheck({ x, y, z }, MapNode(CONTENT_AIR));
		block.setNodeNoCheck({ 3, 4, 5 }, MapNode(t_CONTENT_STONE));
		block.m_shapes.set({ 3, 4, 5 }, s);

		std::stringstream ss(std::ios::binary | std::ios::in | std::ios::out);
		block.serialize(ss, SER_FMT_VER_HIGHEST_WRITE, disk, -1);
		MapBlock back({}, gamedef);
		back.deSerialize(ss, SER_FMT_VER_HIGHEST_WRITE, disk);
		const Shape *got = back.m_shapes.get({ 3, 4, 5 });
		UASSERT(got && *got == s);
		UASSERT(!back.m_shapes.get({ 3, 4, 6 }));
	}

	MapBlock block({}, gamedef);
	block.setNodeNoCheck({ 1, 1, 1 }, MapNode(t_CONTENT_STONE));
	block.m_shapes.set({ 1, 1, 1 }, s);
	std::stringstream ss(std::ios::binary | std::ios::in | std::ios::out);
	block.serialize(ss, 29, true, -1);
	MapBlock back({}, gamedef);
	back.deSerialize(ss, 29, true);
	UASSERT(back.m_shapes.empty());
}

// A shape is its material's: the node becoming something else (set, or
// written through a voxel manipulator) is whole; the same material keeps
// it. A whole shape isn't kept.
void TestShape::testGoesWithItsMaterial(IGameDef *gamedef)
{
	DummyMap map(gamedef, { 0, 0, 0 }, { 0, 0, 0 });
	map.fill({ 0, 0, 0 }, { 0, 0, 0 }, MapNode(t_CONTENT_STONE));
	const v3s16 p(2, 2, 2), q(5, 5, 5);
	const Shape s = rough(7);
	UASSERT(map.setShape(p, s) && map.setShape(q, s));

	map.setNode(p, MapNode(t_CONTENT_STONE, 7)); // a param, not the material
	UASSERT(map.getShape(p));
	map.setNode(p, MapNode(CONTENT_AIR));
	UASSERT(!map.getShape(p));

	MapBlock *block = map.getBlockNoCreate({ 0, 0, 0 });
	VoxelManipulator vm;
	vm.addArea(VoxelArea(v3s16(0), v3s16(MAP_BLOCKSIZE - 1)));
	block->copyTo(vm);
	block->copyFrom(vm);
	UASSERT(map.getShape(q)); // nothing changed
	vm.setNodeNoEmerge(q, MapNode(t_CONTENT_GRASS));
	block->copyFrom(vm);
	UASSERT(!map.getShape(q));

	map.setNode(p, MapNode(t_CONTENT_STONE));
	UASSERT(map.setShape(p, s));
	UASSERT(map.setShape(p, Shape::whole()));
	UASSERT(!map.getShape(p));
	UASSERT(!map.setShape({ 100, 0, 0 }, s)); // its block isn't here
}

// The sixteenth hit is the one inside the face; one across a node's side
// is the next node's.
void TestShape::testAim()
{
	const v3s16 node(4, -2, 7);
	// the top face of sixteenth (3, 15, 9): y at the node's top
	const v3f centre = intToFloat(node, BS);
	const v3f point = centre + v3f((3.5f / STEPS - 0.5f) * BS, 0.5f * BS, (9.5f / STEPS - 0.5f) * BS);
	const Sixteenth at_ = hit(node, point, v3s16(0, 1, 0));
	UASSERT(at_.node == node && at_.cell == v3s16(3, 15, 9));
	const Sixteenth up = beside(at_, v3s16(0, 1, 0));
	UASSERT(up.node == node + v3s16(0, 1, 0) && up.cell == v3s16(3, 0, 9));
	const Sixteenth west = beside(Sixteenth{ node, v3s16(0, 4, 4) }, v3s16(-1, 0, 0));
	UASSERT(west.node == node - v3s16(1, 0, 0) && west.cell == v3s16(15, 4, 4));
	// a point past the node's top (smoothed ground rising): the cell above's
	const Sixteenth risen = hit(node, centre + v3f(0, 0.8f * BS, 0), v3s16(0, 1, 0));
	UASSERT(risen.node == node + v3s16(0, 1, 0) && risen.cell.Y == 4);
}
