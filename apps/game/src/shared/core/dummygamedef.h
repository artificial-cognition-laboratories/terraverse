// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2022 TurkeyMcMac, Jude Melton-Houghton <jwmhjwmh@gmail.com>

#pragma once

#include "src/shared/core/gamedef.h"
#include "src/shared/items/itemdef.h"
#include <memory>
#include "src/shared/content/nodedef.h"
#include "src/shared/items/craftdef.h"
#include "src/server/world/load/database/database-dummy.h"
#if CHECK_CLIENT_BUILD()
#include "src/client/render/meshgen/node_visuals.h"
#endif

class DummyGameDef : public IGameDef {
public:
	DummyGameDef():
		m_itemdef(createItemDefManager()),
		m_nodedef(createNodeDefManager()),
		m_craftdef(createCraftDefManager())
	{
	}

	~DummyGameDef()
	{
		delete m_craftdef;
		delete m_nodedef;
		delete m_itemdef;
	}

	bool isClient() override { return false; }
	IItemDefManager *getItemDefManager() override { return m_itemdef; }
	const NodeDefManager *getNodeDefManager() override { return m_nodedef; }
	NodeDefManager* getWritableNodeDefManager() { return m_nodedef; }
	ICraftDefManager *getCraftDefManager() override { return m_craftdef; }

	u16 allocateUnknownNodeId(const std::string &name) override
	{
		return m_nodedef->allocateDummy(name);
	}


protected:
	IItemDefManager *m_itemdef = nullptr;
	NodeDefManager *m_nodedef = nullptr;
	ICraftDefManager *m_craftdef = nullptr;
};
