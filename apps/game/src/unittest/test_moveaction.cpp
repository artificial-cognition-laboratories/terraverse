// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 Luanti core developers & community

#include "test.h"

#include "mock_inventorymanager.h"
#include "mock_server.h"
#include "mock_serveractiveobject.h"
#include "servermap.h"

// this would better be called "TestInventoryAction"
class TestMoveAction : public TestBase
{
public:
	TestMoveAction() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestMoveAction"; }

	void runTests(IGameDef *gamedef);

	void testMove(ServerActiveObject *obj, IGameDef &gamedef);
	void testMoveFillStack(ServerActiveObject *obj, IGameDef &gamedef);
	void testMoveSomewhere(ServerActiveObject *obj, IGameDef &gamedef);
	void testMoveSomewherePartial(ServerActiveObject *obj, IGameDef &gamedef);

	void testSwap(ServerActiveObject *obj, IGameDef &gamedef);
};

static TestMoveAction g_test_instance;

// Terra Firma (sprint 119): inventories answer moves by their rules alone (what Lua's
// callbacks once allowed, limited or saw went with Lua), so only those are tested here.
void TestMoveAction::runTests(IGameDef *gamedef)
{
	MockServer server(getTestTempDirectory());
	MetricsBackend mb;
	auto null_map = std::unique_ptr<ServerMap>();
	ServerEnvironment server_env(std::move(null_map), &server, &mb);
	MockServerActiveObject obj(&server_env);
	obj.setId(1);

	TEST(testMove, &obj, *gamedef);
	TEST(testMoveFillStack, &obj, *gamedef);
	TEST(testMoveSomewhere, &obj, *gamedef);
	TEST(testMoveSomewherePartial, &obj, *gamedef);
	TEST(testSwap, &obj, *gamedef);
}

static ItemStack parse_itemstack(const char *s)
{
	ItemStack item;
	item.deSerialize(s, nullptr);
	return item;
}

static void apply_action(const char *s, InventoryManager *inv, ServerActiveObject *obj, IGameDef &gamedef)
{
	std::istringstream str(s);
	InventoryAction *action = InventoryAction::deSerialize(str);
	action->apply(inv, obj, &gamedef);
	delete action;
}

void TestMoveAction::testMove(ServerActiveObject *obj, IGameDef &gamedef)
{
	MockInventoryManager inv(gamedef);

	inv.p1.addList("main", 10)->addItem(0, parse_itemstack("default:stone 50"));
	inv.p2.addList("main", 10);

	apply_action("Move 20 player:p1 main 0 player:p2 main 0", &inv, obj, gamedef);

	UASSERT(inv.p1.getList("main")->getItem(0).getItemString() == "default:stone 30");
	UASSERT(inv.p2.getList("main")->getItem(0).getItemString() == "default:stone 20");
}

void TestMoveAction::testMoveFillStack(ServerActiveObject *obj, IGameDef &gamedef)
{
	MockInventoryManager inv(gamedef);

	auto list = inv.p1.addList("main", 10);
	list->changeItem(0, parse_itemstack("default:stone 209"));
	list->changeItem(1, parse_itemstack("default:stone 90")); // 9 free slots

	apply_action("Move 209 player:p1 main 0 player:p1 main 1", &inv, obj, gamedef);

	UASSERT(list->getItem(0).getItemString() == "default:stone 200");
	UASSERT(list->getItem(1).getItemString() == "default:stone 99");

	// Trigger stack swap
	apply_action("Move 200 player:p1 main 0 player:p1 main 1", &inv, obj, gamedef);

	UASSERT(list->getItem(0).getItemString() == "default:stone 99");
	UASSERT(list->getItem(1).getItemString() == "default:stone 200");
}

void TestMoveAction::testMoveSomewhere(ServerActiveObject *obj, IGameDef &gamedef)
{
	MockInventoryManager inv(gamedef);

	inv.p1.addList("main", 10)->addItem(0, parse_itemstack("default:stone 50"));
	InventoryList *list = inv.p2.addList("main", 10);
	list->addItem(0, parse_itemstack("default:brick 10"));
	list->addItem(2, parse_itemstack("default:stone 85"));

	apply_action("MoveSomewhere 50 player:p1 main 0 player:p2 main", &inv, obj, gamedef);

	UASSERT(inv.p2.getList("main")->getItem(0).getItemString() == "default:brick 10");
	UASSERT(inv.p2.getList("main")->getItem(1).getItemString() == "default:stone 36");
	// Partially moved
	UASSERT(inv.p2.getList("main")->getItem(2).getItemString() == "default:stone 99");
}

void TestMoveAction::testMoveSomewherePartial(ServerActiveObject *obj, IGameDef &gamedef)
{
	// "Fail" because the destination list is full.
	MockInventoryManager inv(gamedef);

	InventoryList *src = inv.p1.addList("main", 3);
	src->addItem(0, parse_itemstack("default:brick 10"));
	src->changeItem(1, parse_itemstack("default:stone 111")); // oversized

	InventoryList *dst = inv.p2.addList("main", 1);
	dst->addItem(0, parse_itemstack("default:stone 98"));

	// No free slots to fit
	apply_action("MoveSomewhere 10 player:p1 main 0 player:p2 main", &inv, obj, gamedef);
	UASSERT(inv.p1.getList("main")->getItem(0).getItemString() == "default:brick 10");

	// Only 1 item fits
	apply_action("MoveSomewhere 111 player:p1 main 1 player:p2 main", &inv, obj, gamedef);
	UASSERT(inv.p1.getList("main")->getItem(1).getItemString() == "default:stone 110");
}

void TestMoveAction::testSwap(ServerActiveObject *obj, IGameDef &gamedef)
{
	MockInventoryManager inv(gamedef);

	inv.p1.addList("main", 10)->addItem(0, parse_itemstack("default:stone 50"));
	inv.p2.addList("main", 10)->addItem(0, parse_itemstack("default:brick 60"));

	apply_action("Move 50 player:p1 main 0 player:p2 main 0", &inv, obj, gamedef);

	UASSERT(inv.p1.getList("main")->getItem(0).getItemString() == "default:brick 60");
	UASSERT(inv.p2.getList("main")->getItem(0).getItemString() == "default:stone 50");
}
