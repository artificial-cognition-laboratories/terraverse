// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 Luanti core developers & community

#pragma once

#include "src/shared/base/gamedef.h"
#include "src/shared/game/items/inventory.h"
#include "src/server/world/state/objects/serverinventorymgr.h"


class MockInventoryManager : public ServerInventoryManager
{
public:
	MockInventoryManager(IGameDef &gamedef) :
		p1(gamedef.getItemDefManager()),
		p2(gamedef.getItemDefManager())
	{};

	Inventory *getInventory(const InventoryLocation &loc) override
	{
		if (loc.type == InventoryLocation::PLAYER && loc.name == "p1")
			return &p1;
		if (loc.type == InventoryLocation::PLAYER && loc.name == "p2")
			return &p2;
		return nullptr;
	}
	void setInventoryModified(const InventoryLocation &loc) override {}

	Inventory p1;
	Inventory p2;

};
