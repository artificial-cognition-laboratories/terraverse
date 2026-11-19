// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_v3d.h"
#include <string>

class IGameDef;
class EntitySAO;
class PlayerSAO;
class ServerEnvironment;
namespace piece { class Machine; }

namespace workshop {

/*
	The entities machines are carried by (core:piece): found by their
	object id or their machine's, read, written back, made.
*/
class Entities
{
public:
	Entities(ServerEnvironment &env, IGameDef *gamedef) : m_env(env), m_gamedef(gamedef) {}

	// Throws Refused on an id that isn't a machine's entity.
	EntitySAO &object(u16 id);
	// The entity carrying machine `machine`, round `who`; nothing if it's gone.
	EntitySAO *find(PlayerSAO &who, u64 machine);
	piece::Machine read(EntitySAO &obj) const;
	// Back on its entity (sent); nothing designed or built, it's gone.
	void store(EntitySAO &obj, const piece::Machine &m);
	// A new entity carrying it, at `at` (BS) turned `rotation` (degrees).
	EntitySAO &spawn(v3f at, v3f rotation, const piece::Machine &m);

private:
	ServerEnvironment &m_env;
	IGameDef *m_gamedef;
};

} // namespace workshop
