// Terra Firma: the checks' small probes (sprint 119: were Lua mods). See probe.h.
#include "tests/probe/probe.h"

#include "src/shared/map/map.h"
#include "src/server/app/server.h"
#include "src/server/game/interact.h"
#include "src/server/world/run/rules/matter.h"
#include "src/server/world/terra.h"
#include "src/server/world/state/environment/serverenvironment.h"
#include "src/shared/platform/settings/settings.h"
#include "library/world/terragen/src/world.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <set>

namespace probe {

// apps/game/tests/checks/mods/check.sh: the fixture's /check typed at the console, once before the
// sidecar is crashed and once after it restarts: its commands must answer both times.
void mods(Context &c)
{
	const auto typed = [&c](const std::string &param) {
		const std::string reply = c.server()->command("", nullptr, "check " + param);
		c.say("PROBE /check " + param + " → " + reply);
	};
	c.after(1, [typed] {
		typed("41 words after it");
		typed("x y");
	});
	c.after(5, [typed] { typed("1 again"); });
}

// apps/game/tests/checks/matter.sh: a fire pit, built and lit on a live server, run for simulated
// minutes: clay floor and walls, leaves (tinder) under a log, the leaves lit by an ember.
// Then a family block: peat (soil, its most organic point), heated, burns its organic part.
void matter(Context &c)
{
	const v3s16 P(0, 10, 0);
	const auto at = [P](int dx, int dy, int dz) { return P + v3s16(dx, dy, dz); };
	c.after(1, [&c, at] {
		c.emerge(at(-3, -2, -3), at(3, 6, 3), [&c, at] {
			for (int x = -1; x <= 1; x++)
				for (int z = -1; z <= 1; z++) {
					c.set(at(x, 0, z), "core:clay");
					for (int y = 1; y <= 2; y++)
						if (x != 0 || z != 0)
							c.set(at(x, y, z), "core:clay");
				}
			c.set(at(0, 1, 0), "core:leaves");
			c.set(at(0, 2, 0), "core:log");
			ServerMatter &m = serverMatterStarted(c.server());
			const auto pos = [](v3s16 p) { return sandbox::Pos{ p.X, p.Y, p.Z }; };
			c.say(format("MATTER capacity tinder %.0f log %.0f", m.world->capacity(pos(at(0, 1, 0))),
					m.world->capacity(pos(at(0, 2, 0)))));
			m.world->heat(pos(at(0, 1, 0)), 750); // an ember
			double hottest = 0;
			std::set<std::string> seen;
			for (int second = 1; second <= 1800; second++) {
				m.world->step(1);
				for (const auto &a : m.world->awake(pos(at(-3, -2, -3)), pos(at(3, 8, 3))))
					hottest = std::max(hottest, a.temperature);
				for (const auto &[p, j] : m.world->happened())
					seen.insert(m.table.reactions().at(j).id);
				if (second % 300 == 0)
					c.say(format("MATTER t %d tinder %s log %s log_T %.0f awake %d hottest %.0f", second,
							c.node(at(0, 1, 0)).c_str(), c.node(at(0, 2, 0)).c_str(),
							m.world->temperature(pos(at(0, 2, 0))), (int)m.world->awakeCount(), hottest));
			}
			std::string ids;
			for (const std::string &id : seen)
				ids += (ids.empty() ? "" : ",") + id;
			c.say("MATTER seen " + ids);
			c.say(format("MATTER end tinder %s log %s hottest %.0f awake %d", c.node(at(0, 1, 0)).c_str(),
					c.node(at(0, 2, 0)).c_str(), hottest, (int)m.world->awakeCount()));
			// peat: soil at its most organic point
			int peat = 0;
			for (int i = 0; i < terragen::SOIL_POINTS; i++) {
				const terragen::Soil s = terragen::soilPoint((uint8_t)i);
				if ((double)s.organic / terragen::ONE >= 0.3 && (double)s.clay / terragen::ONE < 0.25) {
					peat = i;
					break;
				}
			}
			const v3s16 Q = at(6, 0, 0);
			c.set(Q, "core:soil", (u8)peat);
			const auto before = m.world->inspect(pos(Q));
			m.world->heat(pos(Q), 900);
			bool burned = false;
			for (int i = 0; i < 300; i++) {
				m.world->step(1);
				for (const auto &[p, j] : m.world->happened()) {
					const std::string &id = m.table.reactions().at(j).id;
					if (p.x == Q.X && p.y == Q.Y && p.z == Q.Z && (id == "wood_burns" || id == "wood_chars"))
						burned = true;
				}
			}
			const std::string substance = before && before->main >= 0 ? m.table.substances().at(before->main).id : "none";
			c.say(format("MATTER soil %s fill %.2f burned %s", substance.c_str(), before ? before->fill : 0.0,
					burned ? "true" : "false"));
			serverMatterSave();
			c.finish();
		});
	});
}

// apps/game/tests/checks/atlas-dig.sh (sprint 30): the ground round the equator emerged; then
// nodes dug one at a time down a shaft, the server's steps after each timed (it relights
// round every change, and sends what changed).
void atlasDig(Context &c)
{
	const double BUDGET = 0.05; // s late: a dig the player doesn't feel
	const v3s16 top = *c.atlasNode(0, 3 * M_PI / 180, 4);
	// the most a server step ran late since the last dig: past the dedicated server's tick
	const float tick = g_settings->getFloat("dedicated_server_step");
	auto longest = std::make_shared<float>(0);
	c.eachStep([longest, tick](float dtime) { *longest = std::max(*longest, dtime - tick); });
	struct State { int layer = 4, pass = 0, fail = 0; float worst = 0; };
	auto s = std::make_shared<State>();
	auto dig = std::make_shared<std::function<void()>>();
	*dig = [&c, s, longest, dig, BUDGET] {
		while (true) {
			if (s->layer < -36) {
				c.say(format("ATLAS worst %.3f s late", s->worst));
				c.say(format("ATLAS pass %d fail %d", s->pass, s->fail));
				c.finish();
				return;
			}
			const std::optional<v3s16> pos = c.atlasNode(0, 3 * M_PI / 180, s->layer);
			if (!pos || !c.cell(*pos)) {
				s->layer--;
				continue;
			}
			*longest = 0;
			interact::dig(c.server(), nullptr, *pos);
			const int layer = s->layer;
			const v3s16 p = *pos;
			c.after(0.5, [&c, s, longest, dig, layer, p, BUDGET] {
				s->worst = std::max(s->worst, *longest);
				if (*longest > BUDGET) {
					s->fail++;
					c.say(format("ATLAS slow after digging layer %d (%d,%d,%d): a step %.3f s late", layer, p.X, p.Y, p.Z, *longest));
				} else {
					s->pass++;
				}
				s->layer--;
				(*dig)();
			});
			return;
		}
	};
	c.after(0, [&c, top, dig] {
		c.emerge(top - v3s16(24, 48, 24), top + v3s16(24, 16, 24), [dig] { (*dig)(); });
	});
}

// apps/game/tests/checks/window.sh (sprint 39): a mark made at the spawn survives its tile leaving
// the map (the window filled with elsewhere, past the pager's hold) and coming back.
void window(Context &c)
{
	const std::string MARK = "core:seam"; // a node the ground never has
	const auto say = [&c](const std::string &s) { c.say("WINDOW " + s); };
	const auto tileOf = [&c](v3s16 p) -> std::optional<planet::Atlas::Rect> {
		for (const planet::Atlas::Tile &t : c.atlas().tiles())
			if (p.X >= t.rect.x && p.X < t.rect.x + t.rect.width && p.Z >= t.rect.z && p.Z < t.rect.z + t.rect.depth)
				return t.rect;
		return std::nullopt;
	};
	c.after(0.5, [&c, MARK, say, tileOf] {
		const v3s16 spawn = c.spawn();
		const planet::Atlas::Cell cell = *c.cell(spawn);
		say(format("spawn (%d,%d,%d), cell %s %d %lld %lld %lld", spawn.X, spawn.Y, spawn.Z, regionName(cell.region), cell.index,
				(long long)cell.i, (long long)cell.j, (long long)cell.k));
		c.emerge(spawn - v3s16(16, 16, 16), spawn + v3s16(16, 16, 16), [&c, MARK, say, tileOf, spawn, cell] {
			const auto [lat, lon] = *c.place(spawn);
			const v3s16 above = spawn + v3s16(0, 3, 0);
			say("above the spawn: " + c.node(above));
			c.set(above, MARK);
			const auto before = tileOf(spawn);
			say(format("tiles in the window: %d; the spawn's at x %d z %d", (int)c.atlas().tiles().size(), before->x, before->z));
			// past the pager's hold, fill the window with elsewhere
			c.after(32, [&c, MARK, say, spawn, cell, lat, lon] {
				int placed = 0;
				bool gone = false;
				for (int n = 0; n <= 400; n++) {
					// a quarter turn and more from the spawn, so never its tile
					const double la = (-60 + (n % 23) * 5) * M_PI / 180;
					const double lo = lon + (90 + (n / 23) * 10) * M_PI / 180;
					try {
						c.atlasNode(la, lo, 0);
					} catch (const std::exception &e) {
						say(std::string("full, as it should be: ") + std::string(e.what()).substr(0, 60));
					}
					placed++;
					const auto now = c.cell(spawn);
					if (!now || !(*now == cell)) {
						gone = true;
						break;
					}
				}
				say(format("asked for %d places elsewhere; the spawn's tile %s; tiles in the window: %d", placed,
						gone ? "left the map" : "STAYED", (int)c.atlas().tiles().size()));
				c.after(31, [&c, MARK, say, spawn, cell, lat, lon, gone] {
					// and back: the same place on the planet
					const v3s16 again = *c.atlasNode(lat, lon, spawn.Y);
					const planet::Atlas::Cell found = *c.cell(again);
					const bool same = found.i == cell.i && found.j == cell.j && found.k == cell.k;
					say(format("the spawn's place is at (%d,%d,%d) now: cell %s %d %lld %lld %lld (%s)", again.X, again.Y,
							again.Z, regionName(found.region), found.index, (long long)found.i, (long long)found.j,
							(long long)found.k, same ? "the same cell" : "ANOTHER CELL"));
					c.emerge(again - v3s16(16, 16, 16), again + v3s16(16, 16, 16), [&c, MARK, say, again, same, gone] {
						const std::string mark = c.node(again + v3s16(0, 3, 0));
						say(std::string("the mark: ") + (mark == MARK ? "kept" : "LOST (" + mark + ")"));
						say(gone && same && mark == MARK ? "pass" : "fail");
						c.finish();
					});
				});
			});
		});
	});
}

} // namespace probe
