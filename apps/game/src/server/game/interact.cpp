// Terraverse: digging, placing and what falls (sprint 119). See interact.h.
#include "src/server/game/interact.h"

#include "src/shared/game/items/inventory.h"
#include "src/shared/game/items/itemdef.h"
#include "src/server/game/kinds.h"
#include "src/server/world/state/objects/entity_sao.h"
#include "src/shared/platform/system/log.h"
#include "src/shared/map/map.h"
#include "src/shared/game/content/nodedef.h"
#include "src/server/players/player_sao.h"
#include "src/shared/game/objects/remoteplayer.h"
#include "src/server/app/server.h"
#include "src/server/world/state/environment/serverenvironment.h"
#include "src/shared/game/items/tool.h"
#include "src/shared/game/pointing/pointedthing.h"
#include <cmath>
#include <random>

namespace interact {

namespace {

float random(float lo, float hi)
{
	static std::mt19937 rng(std::random_device{}());
	return std::uniform_real_distribution<float>(lo, hi)(rng);
}

std::string where(v3s16 p)
{
	return "(" + std::to_string(p.X) + "," + std::to_string(p.Y) + "," + std::to_string(p.Z) + ")";
}

// The colour a block's param2 holds, for one that takes it from a palette: by how many
// of param2's values a colour spans (the rest say which way it faces).
std::optional<u32> colourSpan(ContentParamType2 type)
{
	switch (type) {
	case CPT2_COLOR: return 1;
	case CPT2_COLORED_WALLMOUNTED: return 8;
	case CPT2_COLORED_FACEDIR: return 32;
	case CPT2_COLORED_4DIR: return 4;
	case CPT2_COLORED_DEGROTATE: return 32;
	default: return std::nullopt;
	}
}

// The way a player's facing, as the engine's facedir: along whichever of x and z is the larger.
u8 facedirOf(v3f dir)
{
	if (std::fabs(dir.X) > std::fabs(dir.Z))
		return dir.X < 0 ? 3 : 1;
	return dir.Z < 0 ? 2 : 0;
}

v3f lookDir(PlayerSAO *sao)
{
	const float pitch = sao->getRadLookPitchDep(), yaw = sao->getRadYawDep();
	return v3f(std::cos(pitch) * std::cos(yaw), std::sin(pitch), std::cos(pitch) * std::sin(yaw));
}

bool creative(Server *server, PlayerSAO *sao)
{
	return sao && server->checkPriv(sao->getPlayer()->getName(), "core:creative");
}

// A block that hangs from what's beside it (a flower on its ground): which way that is.
v3s16 attachedTo(const ContentFeatures &f, MapNode n, int rating)
{
	if (rating == 3)
		return v3s16(0, -1, 0); // always the floor
	if (rating == 4)
		return v3s16(0, 1, 0); // always the ceiling
	if (f.param_type_2 == CPT2_WALLMOUNTED || f.param_type_2 == CPT2_COLORED_WALLMOUNTED) {
		static const v3s16 WALL[6] = { { 0, 1, 0 }, { 0, -1, 0 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
		return WALL[std::min<u8>(n.param2 & 7, 5)];
	}
	return v3s16(0, -1, 0);
}

bool supported(ServerEnvironment &env, v3s16 p, MapNode n)
{
	const NodeDefManager *ndef = env.getGameDef()->ndef();
	const ContentFeatures &f = ndef->get(n);
	const int rating = itemgroup_get(f.groups, "attached_node");
	if (rating == 0)
		return true;
	const MapNode on = env.getMap().getNode(p + attachedTo(f, n, rating));
	return on.getContent() == CONTENT_IGNORE || ndef->get(on).walkable;
}

} // namespace

std::vector<ItemStack> drops(Server *server, MapNode n)
{
	const ContentFeatures &f = server->ndef()->get(n);
	if (f.drop) {
		if (f.drop->empty())
			return {};
		ItemStack s;
		s.deSerialize(*f.drop, server->idef());
		return { s };
	}
	ItemStack s(f.name, 1, 0, server->idef());
	if (const std::optional<u32> span = colourSpan(f.param_type_2))
		s.metadata.setString("palette_index", std::to_string(n.param2 / *span * *span));
	return { s };
}

void scatter(ServerEnvironment &env, v3s16 p, const std::vector<ItemStack> &items)
{
	for (const ItemStack &item : items) {
		const v3f at = intToFloat(p, 1) + v3f(random(-0.2f, 0.2f), 0.1f, random(-0.2f, 0.2f));
		if (EntitySAO *obj = spawnItem(env, at, item))
			obj->setVelocity(v3f(random(-0.5f, 0.5f), 1.5f, random(-0.5f, 0.5f)) * BS);
	}
}

bool dig(Server *server, PlayerSAO *digger, v3s16 p)
{
	ServerEnvironment &env = server->getEnv();
	const MapNode n = env.getMap().getNode(p);
	const ContentFeatures &f = server->ndef()->get(n);
	const std::string who = digger ? digger->getPlayer()->getName() : "";
	if (n.getContent() == CONTENT_IGNORE || !f.diggable) {
		if (digger)
			infostream << who << " tried to dig " << f.name << ", which can't be dug, at " << where(p) << std::endl;
		return false;
	}
	if (digger)
		actionstream << who << " digs " << f.name << " at " << where(p) << std::endl;

	const std::vector<ItemStack> given = drops(server, n);
	if (digger && !creative(server, digger)) {
		// the tool wears with use
		ItemStack selected, hand;
		ItemStack tool = digger->getWieldedItem(&selected, &hand);
		const DigParams dp = getDigParams(f.groups, &tool.getToolCapabilities(server->idef(), &hand), tool.wear);
		if (selected.addWear(dp.wear, server->idef()))
			digger->setWieldedItem(selected);
	}
	scatter(env, p, given);
	env.removeNode(p);
	settle(env, p);
	return true;
}

bool place(Server *server, PlayerSAO *placer, ItemStack &stack, const PointedThing &pointed)
{
	const IItemDefManager *idef = server->idef();
	const ItemDefinition &def = stack.getDefinition(idef);
	if (def.type != ITEM_NODE || pointed.type != POINTEDTHING_NODE)
		return false;
	ServerEnvironment &env = server->getEnv();
	const NodeDefManager *ndef = server->ndef();
	const std::string who = placer ? placer->getPlayer()->getName() : "";
	const v3s16 under = pointed.node_undersurface, above = pointed.node_abovesurface;
	bool ok_under, ok_above;
	const MapNode n_under = env.getMap().getNode(under, &ok_under), n_above = env.getMap().getNode(above, &ok_above);
	if (!ok_under || !ok_above) {
		infostream << who << " tried to place " << def.name << " where the map isn't loaded, at " << where(above) << std::endl;
		return false;
	}
	const ContentFeatures &f_under = ndef->get(n_under), &f_above = ndef->get(n_above);
	if (!f_under.buildable_to && !f_above.buildable_to) {
		infostream << who << " tried to place " << def.name << " where something is, at " << where(above) << std::endl;
		return false;
	}
	// into what's pointed at, if it gives way (snow), else beside it
	const v3s16 at = f_under.buildable_to ? under : above;
	const ContentFeatures &f = ndef->get(def.name);
	MapNode n(ndef->getId(def.name));

	if (def.place_param2) {
		n.param2 = *def.place_param2;
	} else if (f.rotates && placer) {
		// facing the way it's put down: on its side against a wall (or, sneaking, the
		// other way round), upside down under a ceiling
		static const u8 WALL[4] = { 9, 18, 7, 12 }, CEILING[4] = { 20, 23, 22, 21 };
		const u8 facing = facedirOf(lookDir(placer));
		bool wall = above.Y == under.Y;
		const bool ceiling = !wall && above.Y < under.Y;
		if (f_under.buildable_to)
			wall = false;
		if (placer->getPlayer()->getPlayerControl().sneak)
			wall = !wall;
		n.param2 = wall ? WALL[facing] : ceiling ? CEILING[facing] : facing;
	} else if (f.param_type_2 == CPT2_FACEDIR || f.param_type_2 == CPT2_COLORED_FACEDIR ||
			f.param_type_2 == CPT2_4DIR || f.param_type_2 == CPT2_COLORED_4DIR) {
		if (placer)
			n.param2 = facedirOf(intToFloat(above, BS) - placer->getBasePosition());
	} else if (f.param_type_2 == CPT2_WALLMOUNTED || f.param_type_2 == CPT2_COLORED_WALLMOUNTED) {
		const v3s16 d = under - above;
		n.param2 = d.Y < 0 ? 1 : d.Y > 0 ? 0 : d.X > 0 ? 3 : d.X < 0 ? 2 : d.Z > 0 ? 5 : 4;
	}
	// a coloured block keeps the colour it was dug with
	if (const std::string index = stack.metadata.getString("palette_index"); !index.empty() && !def.place_param2) {
		if (const std::optional<u32> span = colourSpan(f.param_type_2)) {
			const u32 colour = (u32)stoi(index) / *span;
			n.param2 = (u8)(colour * *span + n.param2 % *span);
		}
	}
	if (!supported(env, at, n)) {
		actionstream << def.name << " can't be placed at " << where(at) << ": nothing holds it there" << std::endl;
		return false;
	}

	actionstream << who << " places node " << def.name << " at " << where(at) << std::endl;
	env.setNode(at, n);
	// in creative, what's in hand never runs out
	if (!creative(server, placer))
		stack.takeItem(1);
	settle(env, at);
	return true;
}

void drop(Server *server, PlayerSAO *placer, ItemStack &stack)
{
	if (stack.empty())
		return;
	const v3f from = placer->getBasePosition() / BS + v3f(0, 1.2f, 0);
	EntitySAO *obj = spawnItem(server->getEnv(), from, stack);
	if (!obj)
		return;
	const v3f dir = lookDir(placer);
	obj->setVelocity(v3f(dir.X * 2.9f, dir.Y * 2.9f + 2, dir.Z * 2.9f) * BS);
	actionstream << placer->getPlayer()->getName() << " drops " << stack.getItemString(false) << std::endl;
	stack.clear();
}

bool settleOne(ServerEnvironment &env, v3s16 p)
{
	Map &map = env.getMap();
	const NodeDefManager *ndef = env.getGameDef()->ndef();
	const MapNode n = map.getNode(p);
	if (n.getContent() == CONTENT_IGNORE || n.getContent() == CONTENT_AIR)
		return false;
	const ContentFeatures &f = ndef->get(n);
	if (itemgroup_get(f.groups, "falling_node") != 0) {
		bool loaded;
		const MapNode below = map.getNode(p + v3s16(0, -1, 0), &loaded);
		if (loaded && below.getContent() != CONTENT_IGNORE) {
			const ContentFeatures &fb = ndef->get(below);
			const bool same = below.getContent() == n.getContent();
			const bool floats = itemgroup_get(f.groups, "float") > 0;
			// a levelled block falls into one of its own that has room
			if (same && fb.param_type_2 == CPT2_LEVELED && below.getLevel(ndef) < below.getMaxLevel(ndef))
				return spawnFalling(env, p, n);
			// else only through what it would fall through: what's not solid, or gives way;
			// a block that floats doesn't sink in a liquid
			if (!same && (!fb.walkable || fb.buildable_to) &&
					(!floats || fb.liquid_type == LIQUID_NONE ||
						(f.liquid_type == LIQUID_SOURCE && fb.liquid_type != LIQUID_SOURCE)))
				return spawnFalling(env, p, n);
		}
	}
	if (!supported(env, p, n)) {
		// what hung from what's gone comes off, as items
		const std::vector<ItemStack> given = drops(env.getServer(), n);
		env.removeNode(p);
		for (const ItemStack &item : given)
			spawnItem(env, intToFloat(p, 1) + v3f(random(-0.25f, 0.25f), random(-0.25f, 0.25f), random(-0.25f, 0.25f)), item);
		return true;
	}
	return false;
}

void settle(ServerEnvironment &env, v3s16 p)
{
	// Each neighbour that falls has its own neighbours looked at, as a walk: below first
	// (likely, and before what's above it), then the sides, the place itself, and above
	// last, so a column over it falls all at once.
	static const v3s16 ROUND[11] = {
		{ -1, -1, 0 }, { 1, -1, 0 }, { 0, -1, -1 }, { 0, -1, 1 }, { 0, -1, 0 },
		{ -1, 0, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 0, 0, 0 }, { 0, 1, 0 },
	};
	struct Visit { v3s16 p; int next; };
	std::vector<Visit> stack;
	int next = 0;
	while (true) {
		stack.push_back({ p, next });
		p += ROUND[next];
		if (settleOne(env, p)) {
			next = 0; // something fell: walk round where it was
			continue;
		}
		// back to where we were, to look at its next neighbour
		while (true) {
			const Visit v = stack.back();
			stack.pop_back();
			p = v.p;
			next = v.next;
			if (stack.empty() && next == 10)
				return;
			if (next < 10)
				break;
		}
		next++;
	}
}

} // namespace interact
