// Terra Firma: what an entity does, by its name (sprint 119: were Lua's registered
// entities). An entity off the world's grid is one of a few native kinds:
//
//   core:piece             a machine (src/workshop), its text kept as its state
//   __builtin:item         something dropped, lying on the ground to be picked up
//   __builtin:falling_node a block falling, which lands as a block
//
// The names are the ones worlds saved them under. An entity of any other name is kept
// as it was saved, inert, and goes when it's punched.
#pragma once

#include "src/shared/base/irrlichttypes_bloated.h"
#include "src/shared/map/block/mapnode.h"
#include <functional>
#include <memory>
#include <string>

class EntitySAO;
class ServerActiveObject;
class ServerEnvironment;
struct ItemStack;
struct CollisionMoveResult;

class EntityKind
{
public:
	virtual ~EntityKind() = default;
	// Its properties set, and what was saved of it (`state`) restored, `dtime_s` after.
	virtual void activate(EntitySAO &self, const std::string &state, u32 dtime_s) = 0;
	// Each step, after it moved (`moved`: how, when it moves by colliding).
	virtual void step(EntitySAO &self, float dtime, const CollisionMoveResult *moved) {}
	// What to save of it.
	virtual std::string state(const EntitySAO &self) const = 0;
	// Punched: true when that's handled (no damage is done).
	virtual bool punch(EntitySAO &self, ServerActiveObject *puncher) { return false; }
};

// The kind of that name; null for one that isn't (kept inert).
std::unique_ptr<EntityKind> entityKind(const std::string &name);

// A kind of the checks' own (src/probe: a walker sent across a seam), by its name.
void registerProbeKind(const std::string &name, std::function<std::unique_ptr<EntityKind>()> make);

// Something dropped at `pos` (nodes), as items lying there: null if it couldn't be added.
EntitySAO *spawnItem(ServerEnvironment &env, v3f pos, const ItemStack &stack);

// The block at `p` falls (it's taken out of the map). False if it couldn't.
bool spawnFalling(ServerEnvironment &env, v3s16 p, MapNode n);
