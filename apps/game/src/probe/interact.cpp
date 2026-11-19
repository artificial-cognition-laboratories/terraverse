// Terra Firma: the check's probe of digging, placing and what falls (sprint 119:
// scripts/interact-check.sh), on a live server: what server/interact and server/kinds do
// in place of Lua's item.lua, item_entity.lua and falling.lua.
#include "probe.h"

#include "inventory.h"
#include "itemdef.h"
#include "map.h"
#include "server.h"
#include "server/interact.h"
#include "server/kinds.h"
#include "server/entity_sao.h"
#include "serverenvironment.h"
#include "util/pointedthing.h"
#include <memory>

namespace probe {

namespace {

// The dropped items lying round p: their items, as their entities hold them.
std::vector<std::string> itemsNear(Context &c, v3s16 p, float radius)
{
	std::vector<ServerActiveObject *> near;
	c.env().getObjectsInsideRadius(near, intToFloat(p, BS), radius * BS, [](ServerActiveObject *o) {
		return !o->isGone() && o->getType() == ACTIVEOBJECT_TYPE_LUAENTITY;
	});
	std::vector<std::string> out;
	for (ServerActiveObject *o : near) {
		auto *e = static_cast<EntitySAO *>(o);
		if (e->getName() == "__builtin:item")
			out.push_back(e->properties().wield_item);
	}
	return out;
}

} // namespace

void interactProbe(Context &c)
{
	struct Tally { int pass = 0, fail = 0; };
	auto t = std::make_shared<Tally>();
	const auto check = [&c, t](bool ok, const std::string &what) {
		(ok ? t->pass : t->fail)++;
		c.say(std::string("INTERACT ") + (ok ? "ok   " : "FAIL ") + what);
	};
	const v3s16 O(0, 10, 0);
	c.after(1, [&c, check, t, O] {
		c.emerge(O - v3s16(8, 4, 8), O + v3s16(8, 12, 8), [&c, check, t, O] {
			// a headless server has no players, so no active blocks: keep these, or what falls
			// and what's dropped is put away mid-air
			for (s16 x = -8; x <= 8; x += 8)
				for (s16 y = -4; y <= 12; y += 8)
					for (s16 z = -8; z <= 8; z += 8)
						c.keep(O + v3s16(x, y, z));
			for (s16 x = -6; x <= 6; x++)
				for (s16 z = -6; z <= 6; z++) {
					c.set(O + v3s16(x, -1, z), "core:stone");
					for (s16 y = 0; y <= 10; y++)
						c.set(O + v3s16(x, y, z), "air");
				}
			// a block dug falls where it was as itself
			const v3s16 dug = O + v3s16(-4, 0, 0);
			c.set(dug, "core:granite", 7);
			check(interact::dig(c.server(), nullptr, dug), "granite can be dug");
			check(c.node(dug) == "air", "the dug block is gone");
			const auto lying = itemsNear(c, dug, 1.5f);
			check(lying.size() == 1 && lying[0].rfind("core:granite", 0) == 0,
					"it lies there as granite (found " + std::to_string(lying.size()) + ")");
			check(!lying.empty() && lying[0].find("palette_index") != std::string::npos,
					"and keeps its colour: " + (lying.empty() ? std::string("none") : lying[0]));
			// a block whose drop is another: grass gives dirt
			const v3s16 grass = O + v3s16(-4, 0, 3);
			c.set(grass, "core:grass");
			interact::dig(c.server(), nullptr, grass);
			const auto dirt = itemsNear(c, grass, 1.5f);
			check(dirt.size() == 1 && dirt[0] == "core:dirt", "grass gives dirt");
			// the seam can't be dug
			const v3s16 seam = O + v3s16(4, 0, 4);
			c.set(seam, "core:seam");
			check(!interact::dig(c.server(), nullptr, seam) && c.node(seam) == "core:seam", "the seam can't be dug");
			// placed against the floor, it goes above what's pointed at; the stack's used
			ItemStack stack("core:log", 5, 0, c.server()->idef());
			PointedThing at;
			at.type = POINTEDTHING_NODE;
			at.node_undersurface = O + v3s16(2, -1, 0);
			at.node_abovesurface = O + v3s16(2, 0, 0);
			check(interact::place(c.server(), nullptr, stack, at), "a log can be placed");
			check(c.node(O + v3s16(2, 0, 0)) == "core:log", "it's where it was put");
			check(stack.count == 4, "and one's gone from the stack");
			// a flower on its ground comes off when the ground goes
			const v3s16 ground = O + v3s16(0, 0, -3), flower = ground + v3s16(0, 1, 0);
			c.set(ground, "core:dirt");
			c.set(flower, "core:poppy");
			interact::dig(c.server(), nullptr, ground);
			check(c.node(flower) == "air", "a poppy comes off when its ground's dug");
			const auto poppy = itemsNear(c, flower, 2);
			bool found = false;
			for (const std::string &i : poppy)
				found = found || i == "core:poppy";
			check(found, "and lies there as a poppy");
			// sand over a hole falls, and lands as sand on the floor
			const v3s16 sand = O + v3s16(4, 4, -2);
			c.set(sand, "core:sand");
			interact::settle(c.env(), sand);
			check(c.node(sand) == "air", "sand over a hole falls");
			c.after(3, [&c, check, t, O, sand] {
				const v3s16 floor = O + v3s16(4, 0, -2);
				check(c.node(floor) == "core:sand", "and lands on the floor as sand (found " + c.node(floor) + ")");
				// items lying together become one stack
				const v3s16 heap = O + v3s16(-2, 0, -4);
				for (int i = 0; i < 3; i++)
					spawnItem(c.env(), intToFloat(heap, 1), ItemStack("core:dirt", 2, 0, c.server()->idef()));
				c.after(3, [&c, check, t, heap] {
					const auto merged = itemsNear(c, heap, 2);
					check(merged.size() == 1 && merged[0] == "core:dirt 6",
							"three drops of dirt lying together become one of 6 (found " + std::to_string(merged.size()) + ")");
					c.say(format("INTERACT pass %d fail %d", t->pass, t->fail));
					c.finish();
				});
			});
		});
	});
}

} // namespace probe
