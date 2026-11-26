// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tests/unit/test.h"

#include "src/server/world/run/machines/piece/object.h"
#include "src/server/world/run/machines/piece/header.h"
#include "src/server/world/run/sculpt/sculpt.h"
#include "src/shared/base/dummygamedef.h"
#include "src/shared/game/content/nodedef.h"
#include "src/shared/game/objects/object_properties.h"
#include "src/shared/util/data/serialize.h"
#include <sstream>

using namespace piece;

class TestPiece : public TestBase {
public:
	TestPiece() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestPiece"; }

	void runTests(IGameDef *gamedef);

	void testWrittenAndReadBack();
	void testAnObject();
	void testByName();
	void testSculptedInItsGrid();
	void testPointedAt();
	void testHollowed();
	void testPainted();
	void testSentWithItsEntity();
};

static TestPiece g_test_instance;

void TestPiece::runTests(IGameDef *gamedef)
{
	TEST(testWrittenAndReadBack);
	TEST(testAnObject);
	TEST(testByName);
	TEST(testSculptedInItsGrid);
	TEST(testPointedAt);
	TEST(testHollowed);
	TEST(testPainted);
	TEST(testSentWithItsEntity);
}

namespace {

struct Game
{
	DummyGameDef gamedef;
	content_t planks, iron;

	Game()
	{
		NodeDefManager *ndef = gamedef.getWritableNodeDefManager();
		for (auto [name, id] : { std::pair<const char *, content_t *>{ "test:planks", &planks }, { "test:iron", &iron } }) {
			ContentFeatures f;
			f.name = name;
			f.walkable = true;
			f.drawtype = NDT_NORMAL;
			*id = ndef->set(f.name, std::move(f));
		}
	}
};

shape::Shape notch()
{
	shape::Shape s = shape::Shape::whole();
	for (s16 x = 0; x < 8; x++)
		for (s16 y = 8; y < 16; y++)
			s.set({ x, y, 3 }, false);
	return s;
}

} // namespace

// A piece's blocks, params and cuts survive its text; an empty piece is empty.
void TestPiece::testWrittenAndReadBack()
{
	Game g;
	Piece p(&g.gamedef);
	UASSERT(p.empty());
	p.map().setNode({ 0, 0, 0 }, MapNode(g.planks));
	p.map().setNode({ 1, 0, 0 }, MapNode(g.iron, 0, 3));
	p.map().setNode({ 15, 15, 15 }, MapNode(g.planks));
	p.map().setShape({ 1, 0, 0 }, notch());

	Piece back(&g.gamedef);
	back.read(p.write());
	UASSERT(!back.empty());
	UASSERT(back.map().getNode({ 0, 0, 0 }).getContent() == g.planks);
	UASSERT(back.map().getNode({ 1, 0, 0 }).getContent() == g.iron);
	UASSERT(back.map().getNode({ 1, 0, 0 }).getParam2() == 3);
	UASSERT(back.map().getNode({ 15, 15, 15 }).getContent() == g.planks);
	UASSERT(back.map().getNode({ 2, 0, 0 }).getContent() == CONTENT_AIR);
	UASSERT(back.map().getShape({ 1, 0, 0 }) && *back.map().getShape({ 1, 0, 0 }) == notch());
	UASSERT(back.write() == p.write());

	UASSERT(boxOf(p.write()) && boxOf(p.write())->MinEdge == v3f(-0.5f) && boxOf(p.write())->MaxEdge == v3f(15.5f));
}

// An object keeps its id, its design and what's built; a new one has an
// id of its own, a copy renewed another. Built, nothing's left; what's
// designed and not built is the difference, to the sixteenth. A piece's
// text from before objects (TFP1, TFP2) is one built as designed.
void TestPiece::testAnObject()
{
	Game g;
	Object o(&g.gamedef), other(&g.gamedef);
	UASSERT(o.id() != 0 && o.id() != other.id() && o.empty());
	o.design().map().setNode({ 0, 0, 0 }, MapNode(g.planks));
	o.design().map().setNode({ 1, 0, 0 }, MapNode(g.iron));
	o.built().map().setNode({ 0, 0, 0 }, MapNode(g.planks));
	o.built().map().setNode({ 1, 0, 0 }, MapNode(g.iron));
	o.built().map().setShape({ 1, 0, 0 }, notch());

	// unbuilt: all of (2, 0, 0) is designed, not built; of (1, 0, 0), the notch
	o.design().map().setNode({ 2, 0, 0 }, MapNode(g.planks));
	Piece left = o.unbuilt();
	UASSERT(left.map().getNode({ 0, 0, 0 }).getContent() == CONTENT_AIR);
	UASSERT(left.map().getNode({ 2, 0, 0 }).getContent() == g.planks && !left.map().getShape({ 2, 0, 0 }));
	const shape::Shape *notched = left.map().getShape({ 1, 0, 0 });
	UASSERT(notched && notched->has({ 0, 8, 3 }) && !notched->has({ 0, 0, 0 }) && !notched->has({ 8, 8, 3 }));

	Object back(&g.gamedef);
	back.read(o.write());
	UASSERT(back.id() == o.id() && back.write() == o.write());
	UASSERT(header(o.write()).id == o.id());
	UASSERT(header(o.write()).built->MaxEdge == v3f(1.5f, 0.5f, 0.5f));
	UASSERT(header(o.write()).designed->MaxEdge == v3f(2.5f, 0.5f, 0.5f));
	back.build();
	UASSERT(back.unbuilt().empty() && back.built().write() == back.design().write());
	back.renew();
	UASSERT(back.id() != o.id());

	// TFP1, and TFP2 (an id and a ghost flag first): built as designed
	std::ostringstream body(std::ios::binary);
	writeU16(body, 1);
	body << serializeString16("test:planks");
	writeU16(body, 1);
	for (u8 b : { 2, 0, 0 })
		writeU8(body, b);
	writeU16(body, 0);
	writeU8(body, 0);
	writeU8(body, 0);
	std::ostringstream two(std::ios::binary);
	two.write("TFP2", 4);
	writeU64(two, 77);
	writeU8(two, 1);
	for (const std::string &text : { "TFP1" + body.str(), two.str() + body.str() }) {
		Object old(&g.gamedef);
		old.read(text);
		UASSERT(old.id() != 0 && !header(text).id);
		UASSERT(old.built().map().getNode({ 2, 0, 0 }).getContent() == g.planks && old.unbuilt().empty());
	}
}

// Written by name: read where the ids differ, it's the same blocks; a name
// the game hasn't got, or text that isn't a piece, throws.
void TestPiece::testByName()
{
	Game g;
	Piece p(&g.gamedef);
	p.map().setNode({ 2, 3, 4 }, MapNode(g.iron));
	const std::string text = p.write();

	DummyGameDef other;
	NodeDefManager *ndef = other.getWritableNodeDefManager();
	for (const char *name : { "test:sand", "test:gravel", "test:iron" }) {
		ContentFeatures f;
		f.name = name;
		f.walkable = true;
		f.drawtype = NDT_NORMAL;
		ndef->set(f.name, std::move(f));
	}
	content_t iron;
	UASSERT(ndef->getId("test:iron", iron) && iron != g.iron);
	Piece there(&other);
	there.read(text);
	UASSERT(there.map().getNode({ 2, 3, 4 }).getContent() == iron);

	DummyGameDef bare;
	Piece nowhere(&bare);
	EXCEPTION_CHECK(SerializationError, nowhere.read(text));
	EXCEPTION_CHECK(SerializationError, nowhere.read("not a piece"));
}

// The tools work on a piece as on the world: a cube dug out of its block,
// a block placed beside it, and undone.
void TestPiece::testSculptedInItsGrid()
{
	Game g;
	Piece p(&g.gamedef);
	p.map().setNode({ 0, 0, 0 }, MapNode(g.planks));
	sculpt::Sculptor sculptor(p.map());
	const shape::Sixteenth top{ v3s16(0, 0, 0), v3s16(8, 15, 8) };
	const sculpt::Edit dug = sculptor.apply({ sculpt::Tool::Box, 4, top, v3s16(0, 1, 0), sculpt::Act::Cut, std::nullopt }, CONTENT_IGNORE);
	UASSERT(p.map().getShape({ 0, 0, 0 }) && !p.map().getShape({ 0, 0, 0 })->has({ 8, 15, 8 }));
	const shape::Sixteenth side{ v3s16(0, 0, 0), v3s16(15, 4, 4) };
	sculptor.apply({ sculpt::Tool::Box, 16, side, v3s16(1, 0, 0), sculpt::Act::Add, std::nullopt }, g.iron);
	UASSERT(p.map().getNode({ 1, 0, 0 }).getContent() == g.iron);
	dug.restore(p.map());
	UASSERT(!p.map().getShape({ 0, 0, 0 }));
	// past its grid nothing is placed: it's at most SIDE a side
	const shape::Sixteenth west{ v3s16(0, 0, 0), v3s16(0, 4, 4) };
	sculptor.apply({ sculpt::Tool::Box, 16, west, v3s16(-1, 0, 0), sculpt::Act::Add, std::nullopt }, g.iron, Piece::area());
	UASSERT(p.map().getNode({ -1, 0, 0 }).getContent() == CONTENT_AIR);

	Piece back(&g.gamedef);
	back.read(p.write());
	UASSERT(back.map().getNode({ 1, 0, 0 }).getContent() == g.iron);
}

// Pointed at in its own grid, a piece is hit where it's cut: through the
// notch to the block behind it.
void TestPiece::testPointedAt()
{
	Game g;
	Piece p(&g.gamedef);
	p.map().setNode({ 0, 0, 0 }, MapNode(g.planks));
	p.map().setNode({ 3, 0, 0 }, MapNode(g.planks));
	const core::line3d<f32> along(v3f(-5, 0, 0) * BS, v3f(10, 0, 0) * BS);
	std::optional<Piece::Hit> hit = p.raycast(along);
	UASSERT(hit && hit->node == v3s16(0, 0, 0) && hit->normal == v3s16(-1, 0, 0));

	// cut away the near block's middle row along x: the ray goes through it
	shape::Shape s = shape::Shape::whole();
	for (s16 x = 0; x < 16; x++)
		for (s16 y = 6; y < 10; y++)
			for (s16 z = 6; z < 10; z++)
				s.set({ x, y, z }, false);
	p.map().setShape({ 0, 0, 0 }, s);
	hit = p.raycast(along);
	UASSERT(hit && hit->node == v3s16(3, 0, 0));
	UASSERT(!p.raycast({ v3f(-5, 3, 0) * BS, v3f(10, 3, 0) * BS }));
}

// An entity carrying a piece is selected and stood on as far as its blocks
// go, and its piece crosses the wire with its properties.
void TestPiece::testSentWithItsEntity()
{
	Game g;
	Object o(&g.gamedef);
	o.design().map().setNode({ 0, 0, 0 }, MapNode(g.planks));
	o.design().map().setNode({ 2, 1, 0 }, MapNode(g.iron));
	ObjectProperties props;
	props.visual = OBJECTVISUAL_PIECE;
	// nothing built: selected as designed, passed through
	props.piece = o.write();
	props.validate();
	UASSERT(props.selectionbox == aabb3f(-0.5f, -0.5f, -0.5f, 2.5f, 1.5f, 0.5f));
	UASSERT(!props.physical && !props.collideWithObjects);
	// built (one block of it): stood on as far as that goes
	o.built().map().setNode({ 0, 0, 0 }, MapNode(g.planks));
	props.piece = o.write();
	props.validate();
	UASSERT(props.collisionbox == aabb3f(-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f));
	UASSERT(props.selectionbox == aabb3f(-0.5f, -0.5f, -0.5f, 2.5f, 1.5f, 0.5f));
	UASSERT(props.physical && props.collideWithObjects);

	std::ostringstream os(std::ios::binary);
	props.serialize(os);
	std::istringstream is(os.str(), std::ios::binary);
	ObjectProperties back;
	back.deSerialize(is);
	UASSERT(back.visual == OBJECTVISUAL_PIECE && back.piece == props.piece);
}

// Hollowed, a solid 3³ keeps a wall as thick as asked all round, and
// nothing further in: its middle node's gone; undone, it's whole again.
void TestPiece::testHollowed()
{
	Game g;
	Piece p(&g.gamedef);
	for (s16 x = 0; x < 3; x++)
		for (s16 y = 0; y < 3; y++)
			for (s16 z = 0; z < 3; z++)
				p.map().setNode({ x, y, z }, MapNode(g.planks));
	const int wall = 4;
	const sculpt::Edit edit = sculpt::Sculptor(p.map()).hollow(Piece::area(), wall);
	size_t kept = 0;
	p.each([&](v3s16 at, MapNode, const shape::Shape *s) {
		for (s16 x = 0; x < 16; x++)
			for (s16 y = 0; y < 16; y++)
				for (s16 z = 0; z < 16; z++)
					kept += !s || s->has({ x, y, z });
	});
	const size_t side = 3 * 16, inner = side - 2 * wall;
	UASSERTEQ(size_t, kept, side * side * side - inner * inner * inner);
	UASSERT(p.map().getNode({ 1, 1, 1 }).getContent() == CONTENT_AIR);
	UASSERTEQ(size_t, edit.was.size(), size_t(27)); // each had sixteenths deeper in than the wall
	edit.restore(p.map());
	UASSERT(p.map().getNode({ 1, 1, 1 }).getContent() == g.planks && !p.map().getShape({ 0, 0, 0 }));
}

// Painted, each node the shape touches is the new material, cut as it
// was; a node it misses keeps its own.
void TestPiece::testPainted()
{
	Game g;
	Piece p(&g.gamedef);
	p.map().setNode({ 0, 0, 0 }, MapNode(g.planks));
	p.map().setNode({ 3, 0, 0 }, MapNode(g.planks));
	p.map().setShape({ 0, 0, 0 }, notch());
	sculpt::Sculptor(p.map()).apply({ sculpt::Tool::Box, 4, { v3s16(0, 0, 0), v3s16(8, 15, 8) }, v3s16(0, 1, 0),
		sculpt::Act::Paint, std::nullopt }, g.iron);
	UASSERT(p.map().getNode({ 0, 0, 0 }).getContent() == g.iron);
	UASSERT(p.map().getShape({ 0, 0, 0 }) && *p.map().getShape({ 0, 0, 0 }) == notch());
	UASSERT(p.map().getNode({ 3, 0, 0 }).getContent() == g.planks);
}
