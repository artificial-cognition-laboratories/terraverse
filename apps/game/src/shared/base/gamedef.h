// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <string>
#include <vector>
#include "src/shared/base/irrlichttypes.h"

class IItemDefManager;
class NodeDefManager;
class ICraftDefManager;
class IRollbackManager;
struct SubgameSpec;

/*
	An interface for fetching game-global definitions like tool and
	mapnode properties
*/
class IGameDef
{
public:
	// These are thread-safe IF they are not edited while running threads.
	// Thus, first they are set up and then they are only read.
	virtual IItemDefManager* getItemDefManager()=0;
	virtual const NodeDefManager* getNodeDefManager()=0;
	virtual ICraftDefManager* getCraftDefManager()=0;

	// Used for keeping track of names/ids of unknown nodes
	virtual u16 allocateUnknownNodeId(const std::string &name)=0;

	// Only usable on the server, and NOT thread-safe. It is usable from the
	// environment thread.
	virtual IRollbackManager* getRollbackManager() { return NULL; }

	// Shorthands
	// TODO: these should be made const-safe so that a const IGameDef* is
	//       actually usable
	IItemDefManager  *idef()     { return getItemDefManager(); }
	const NodeDefManager  *ndef() { return getNodeDefManager(); }
	ICraftDefManager *cdef()     { return getCraftDefManager(); }
	IRollbackManager *rollback() { return getRollbackManager(); }

	virtual const SubgameSpec* getGameSpec() const { return nullptr; }
	virtual std::string getWorldPath() const { return ""; }
	virtual std::string getModDataPath() const { return ""; }

	virtual bool isClient() = 0;
};
