// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/server/world/run/rules/matter.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/platform/system/filesys.h"
#include "library/matter/fill/fill.h"
#include "src/shared/map/map.h"
#include "src/shared/game/content/nodedef.h"
#include "library/world/planet/atlas.h"
#include "src/shared/platform/system/porting.h"
#include "src/server/app/server.h"
#include "src/server/world/state/environment/serverenvironment.h"
#include "library/geometry/shape/shape.h"
#include "library/world/terragen/src/code/worlds.h"
#include "src/shared/game/items/itemgroup.h"
#include "src/shared/platform/system/log.h"
#include "src/server/world/run/machines/workshop/motion.h"
#include "src/shared/game/content/particles.h"
#include "src/shared/game/objects/remoteplayer.h"
#include "src/server/players/player_sao.h"
#include "src/server/world/terra.h"
#include <json/json.h>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

using namespace sandbox;

/*
	The server's world matter: made at the first registration, loaded from
	disk at its first use (once every kind is registered), and from then on
	closed to new kinds. Server Lua runs on one thread.
*/

static std::unique_ptr<ServerMatter> s_matter;

int64_t matterCube(Pos p)
{
	auto c = [](int32_t v) { return (int64_t)((v >= 0 ? v : v - 15) / 16) & 0x1FFFFF; };
	return (c(p.x) << 42) | (c(p.y) << 21) | c(p.z);
}

// Sprint 81: a node's open space. Partly there (cut, or smoothed ground:
// Map::solidShape), its empty sixteenths; whole, all of it where it isn't
// stood on (air, a plant, the sea's water, a levelled heap), none where it
// is; unloaded, none.
struct Open
{
	std::optional<shape::Shape> solid;
	bool whole_open;
	bool at(v3s16 sixteenth) const { return solid ? !solid->has(sixteenth) : whole_open; }
};

static Open openOf(Map &map, const NodeDefManager *ndef, v3s16 p)
{
	Open o{ map.solidShape(p), false };
	if (!o.solid) {
		const MapNode n = map.getNode(p);
		const ContentFeatures &f = ndef->get(n);
		o.whole_open = n.getContent() != CONTENT_IGNORE && (!f.walkable || f.param_type_2 == CPT2_LEVELED);
	}
	return o;
}

sandbox::Room matterRoomAt(Map &map, const NodeDefManager *ndef, v3s16 p)
{
	const Open o = openOf(map, ndef, p);
	if (!o.solid)
		return o.whole_open ? sandbox::Room::open() : sandbox::Room::closed();
	sandbox::Room r;
	for (int y = 0; y < 16; y++) {
		uint16_t n = 0;
		for (int z = 0; z < 16; z++)
			for (int x = 0; x < 16; x++)
				n += o.solid->has(v3s16(x, y, z)) ? 0 : 1;
		r.layers[y] = n;
	}
	return r;
}

// Which layers are open through a side (0..3: +x −x +z −z): where both
// nodes are empty somewhere along the face they share.
static uint16_t faceAt(Map &map, const NodeDefManager *ndef, v3s16 p, int side)
{
	static const v3s16 DIRS[4] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
	const v3s16 d = DIRS[side];
	const Open a = openOf(map, ndef, p), b = openOf(map, ndef, p + d);
	if (!a.solid && !b.solid) // whole on both sides: all open or none
		return a.whole_open && b.whole_open ? 0xFFFF : 0;
	uint16_t mask = 0;
	for (int y = 0; y < 16; y++)
		for (int i = 0; i < 16 && !(mask & (1 << y)); i++) {
			// the sixteenth on p's side of the face, and the one facing it
			const v3s16 mine = d.X ? v3s16(d.X > 0 ? 15 : 0, y, i) : v3s16(i, y, d.Z > 0 ? 15 : 0);
			const v3s16 theirs = d.X ? v3s16(d.X > 0 ? 0 : 15, y, i) : v3s16(i, y, d.Z > 0 ? 0 : 15);
			if (a.at(mine) && b.at(theirs))
				mask |= 1 << y;
		}
	return mask;
}

ServerMatter &serverMatter(Server *server)
{
	if (s_matter)
		return *s_matter;
	auto m = std::make_unique<ServerMatter>(ServerMatter{
		matter::Matter::load(porting::path_share + DIR_DELIM "matter"), nullptr, server,
		server->getWorldPath() + DIR_DELIM "matter.bin", {}, {}, {}, {} });
	ServerMatter *sm = m.get();
	WorldMatter::Ground ground{
		[sm](Pos p) {
			MapNode n = sm->server->getEnv().getMap().getNode(v3s16(p.x, p.y, p.z));
			content_t c = n.getContent();
			if (c >= sm->kind_of.size() || sm->kind_of[c] < 0)
				return -1; // unloaded (ignore) and unregistered: sealed
			// a family's param2 is which of its points it is
			return sm->kind_of[c] + std::min<int32_t>(n.param2, sm->points_of[c] - 1);
		},
		[sm](Pos p, int32_t kind, double level) {
			const auto &[c, param2] = sm->node_of.at(kind);
			const ContentFeatures &f = sm->server->ndef()->get(c);
			// a levelled block keeps how full it is in param2, in sixty-fourths (sprint 80)
			u8 p2 = param2;
			if (f.param_type_2 == CPT2_LEVELED)
				p2 = (u8)std::clamp<int>((int)std::lround(level * 64), 1, 64);
			sm->server->getEnv().swapNode(v3s16(p.x, p.y, p.z), MapNode(c, 0, p2));
		},
		[sm](Pos p) {
			auto it = sm->ambient.find(matterCube(p));
			return it != sm->ambient.end() ? it->second : 288.15; // a temperate default until the weather says
		},
		[sm](Pos p) {
			MapNode n = sm->server->getEnv().getMap().getNode(v3s16(p.x, p.y, p.z));
			const ContentFeatures &f = sm->server->ndef()->get(n);
			return f.param_type_2 == CPT2_LEVELED ? n.getLevel(sm->server->ndef()) / 64.0 : 1.0;
		},
		[sm](Pos p) { return matterRoomAt(sm->server->getEnv().getMap(), sm->server->ndef(), v3s16(p.x, p.y, p.z)); },
		[sm](Pos p, int side) { return faceAt(sm->server->getEnv().getMap(), sm->server->ndef(), v3s16(p.x, p.y, p.z), side); },
		[sm](Pos p) {
			const fill::Fill *f = sm->server->getEnv().getMap().getFill(v3s16(p.x, p.y, p.z));
			sandbox::Liquid l;
			if (f) {
				// a fill names a substance the table may no longer have: then it's none
				const auto &all = sm->table.substances();
				const std::string &id = fill::name(f->liquid);
				for (size_t s = 0; s < all.size(); s++)
					if (all[s].id == id) {
						l.substance = (int32_t)s;
						l.share = f->amount / 255.0;
					}
			}
			return l;
		},
		[sm](Pos p, int32_t substance, double share, uint32_t colour) {
			fill::Fill f;
			if (substance >= 0) {
				f.liquid = fill::liquid(sm->table.substances().at(substance).id);
				f.amount = (u8)std::clamp<long>(std::lround(share * 255), 0, 255);
				f.r = (colour >> 16) & 0xFF;
				f.g = (colour >> 8) & 0xFF;
				f.b = colour & 0xFF;
			}
			sm->server->getEnv().getMap().setFill(v3s16(p.x, p.y, p.z), f);
		},
	};
	// how wet the ground is, as the weather says (sprint 111); dry where it hasn't
	ground.wetness = [sm](Pos p) {
		auto it = sm->wetness.find(matterCube(p));
		return it != sm->wetness.end() ? it->second : 0.0;
	};
	// a river's or lake's water as generated, kept full (sprint 115)
	ground.held = [sm](Pos p) {
		return (sm->server->getEnv().getMap().getNode(v3s16(p.x, p.y, p.z)).getParam2() & fill::HELD) != 0;
	};
	// each body's own air, from its card (sprint 96): a fire on the Moon starves
	ground.carve = [sm](Pos p, int cells, unsigned exposed, bool melting) {
		Map &map = sm->server->getEnv().getMap();
		map.setShape(v3s16(p.x, p.y, p.z), shape::melted(map.getShape(v3s16(p.x, p.y, p.z)), cells, exposed, melting));
	};
	std::vector<WorldMatter::Air> airs;
	const planet::Atlas *atlas = sm->server->atlas();
	if (atlas) {
		for (const terragen::BodyWorld &b : sm->server->bodyWorlds())
			airs.push_back({ b.surroundings.pressure, b.surroundings.gravity, b.surroundings.gases });
		ground.air = [atlas](Pos p) {
			const std::optional<int> body = atlas->bodyAt({ p.x, p.y, p.z });
			return (size_t)body.value_or(0); // (off the atlas: the home's, as the seams are)
		};
	} else {
		airs.push_back({ 101325, 9.81, { { "nitrogen", 0.79 }, { "oxygen", 0.21 } } });
	}
	m->world = std::make_unique<WorldMatter>(WorldMatter::Opts{ m->table, airs, ground });
	s_matter = std::move(m);
	return *s_matter;
}

// The world matter once every kind is in: loads what was awake when the server last stopped.
ServerMatter &serverMatterStarted(Server *server)
{
	ServerMatter &m = serverMatter(server);
	if (m.started)
		return m;
	m.started = true;
	std::ifstream in(m.path, std::ios::binary);
	if (in) {
		std::stringstream bytes;
		bytes << in.rdbuf();
		m.world->load(bytes.str());
	}
	return m;
}


ServerMatter *serverMatterIfMade()
{
	return s_matter.get();
}

void serverMatterShutdown()
{
	s_matter.reset();
}

int32_t serverMatterBlock(Server *server, const std::string &node, const MatterBlock &b)
{
	ServerMatter &m = serverMatter(server);
	if (m.started)
		throw matter::MatterError("the world's matter has started: " + node + " is too late to register");
	if (b.matter.empty())
		throw matter::MatterError(node + " needs a matter");
	const content_t c = server->ndef()->getId(node);
	if (c == CONTENT_IGNORE)
		throw matter::MatterError("no node called " + node);
	// levelled: its definition keeps how full it is in param2
	const bool leveled = server->ndef()->get(c).param_type_2 == CPT2_LEVELED;
	const WorldMatter::Spec spec{ node, b.matter, {}, b.share, b.state, b.form, b.flowing, b.vessel, leveled, b.loose };
	const int32_t kind = m.world->kind(spec);
	m.plain.push_back(spec);
	if (m.kind_of.size() <= c) {
		m.kind_of.resize(c + 1, -1);
		m.points_of.resize(c + 1, 1);
	}
	m.kind_of[c] = kind;
	m.points_of[c] = 1;
	m.node_of.push_back({ c, 0 });
	return kind;
}

int32_t serverMatterFamily(Server *server, const std::string &node, const MatterFamily &f)
{
	ServerMatter &m = serverMatter(server);
	if (m.started)
		throw matter::MatterError("the world's matter has started: " + node + " is too late to register");
	const content_t c = server->ndef()->getId(node);
	if (c == CONTENT_IGNORE)
		throw matter::MatterError("no node called " + node);
	const size_t count = f.points.size();
	if (count < 1 || count > 256)
		throw matter::MatterError(node + " has 1..256 points (its param2)");
	if (!f.holds.empty() && f.holds.size() != count)
		throw matter::MatterError(node + "'s holds are one a point");
	int32_t first = -1;
	for (size_t i = 0; i < count; i++) {
		std::vector<std::pair<std::string, double>> parts = f.points[i];
		std::sort(parts.begin(), parts.end()); // the mix is the same however it was listed
		WorldMatter::Spec spec{ node + "#" + std::to_string(i), "", parts, f.share, f.state, f.form && i == 0, false };
		spec.holds = f.holds.empty() ? 0 : f.holds[i];
		const int32_t kind = m.world->kind(spec);
		if (first < 0)
			first = kind;
		m.node_of.push_back({ c, (u8)i });
	}
	if (m.kind_of.size() <= c) {
		m.kind_of.resize(c + 1, -1);
		m.points_of.resize(c + 1, 1);
	}
	m.kind_of[c] = first;
	m.points_of[c] = (int32_t)count;
	return first;
}

void serverMatterSave()
{
	if (!s_matter || !s_matter->started)
		return; // nothing has run: nothing to save
	const std::string tmp = s_matter->path + ".tmp";
	{
		std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
		const std::string bytes = s_matter->world->save();
		out.write(bytes.data(), bytes.size());
		if (!out)
			throw BaseException("matter: couldn't write " + tmp);
	}
	if (std::rename(tmp.c_str(), s_matter->path.c_str()) != 0)
		throw BaseException("matter: couldn't replace " + s_matter->path);
}

namespace {

// What a block takes before it fails (Pa) and a crack through it (J/m²), by how
// it breaks, for a block whose substance doesn't say. Sand's only held by being damp.
struct ByGroup
{
	const char *group;
	float tension[3], compression[3], toughness[3]; // by the group's level, 1..3
};
const ByGroup BY_GROUP[] = {
	{ "cracky", { 1e7f, 5e6f, 2e6f }, { 1e8f, 5e7f, 2e7f }, { 100, 100, 100 } },
	{ "choppy", { 4e7f, 4e7f, 4e7f }, { 3e7f, 3e7f, 3e7f }, { 1e4f, 1e4f, 1e4f } },
	{ "crumbly", { 3e4f, 3e4f, 3e4f }, { 3e5f, 3e5f, 3e5f }, { 100, 100, 100 } },
	{ "snappy", { 1e5f, 1e5f, 1e5f }, { 1e5f, 1e5f, 1e5f }, { 1e3f, 1e3f, 1e3f } },
};

} // namespace

void terraRegisterMatter(Server *server)
{
	const NodeDefManager *ndef = server->ndef();
	ServerMatter &m = serverMatter(server);
	serverMatterBlock(server, "air", MatterBlock{ "air", 1, matter::State::Solid, true });

	// by name, so kinds (and which block is the form of what) come out the same every
	// time; the world's own blocks first, so where two are the form of the same matter
	// theirs wins (lava cools to basalt, not to a feldspar block)
	std::vector<std::pair<std::string, content_t>> world, generated;
	for (content_t c = 0; c < ndef->size(); c++) {
		const ContentFeatures &f = ndef->get(c);
		if (f.name.empty() || f.name == "air" || (!f.matter && !f.family))
			continue;
		(f.matter_generated ? generated : world).push_back({ f.name, c });
	}
	std::sort(world.begin(), world.end());
	std::sort(generated.begin(), generated.end());
	for (const auto *list : { &world, &generated }) {
		for (const auto &[name, c] : *list) {
			const ContentFeatures &f = ndef->get(c);
			try {
				if (f.family) {
					serverMatterFamily(server, name, *f.family);
				} else {
					MatterBlock b = *f.matter;
					// a liquid the engine runs, not the world's matter
					b.flowing = f.liquid_type != LIQUID_NONE;
					serverMatterBlock(server, name, b);
				}
			} catch (const std::runtime_error &e) {
				throw BaseException("matter: " + name + ": " + e.what());
			}
		}
	}

	// what each solid block weighs and how it holds in a machine (sprint 57): its
	// matter's density; its substance's grip and strength by its share, or how it
	// breaks; what `_machine` says overrides both
	std::vector<std::string> unweighed, unjudged;
	for (content_t c = 0; c < ndef->size(); c++) {
		const ContentFeatures &f = ndef->get(c);
		if (f.name.empty() || f.name == "air" || f.name == "ignore" || f.name == "unknown" || !f.walkable)
			continue;
		const double density = c < m.kind_of.size() && m.kind_of[c] >= 0 ? m.world->density(m.kind_of[c]) : 0;
		if (!(density > 0)) {
			unweighed.push_back(f.name);
			continue;
		}
		const MatterMachine machine = f.machine.value_or(MatterMachine{});
		const matter::Substance *substance = nullptr;
		if (f.matter) {
			for (const matter::Substance &s : m.table.substances())
				if (s.id == f.matter->matter)
					substance = &s;
		}
		const double share = f.matter ? f.matter->share : 1;
		const bool loose = itemgroup_get(f.groups, "pile_bearing") > 0;
		const bool slippery = itemgroup_get(f.groups, "slippery") > 0;
		workshop::Material mat{ machine.mass.value_or((float)density),
				machine.friction.value_or(slippery ? 0.1f : loose ? 0.35f :
						substance && substance->friction ? (float)*substance->friction : 0.6f) };
		if (!machine.role.empty()) {
			size_t r = 0;
			while (r < size_t(workshop::Role::Count) && machine.role != workshop::ROLES[r].id)
				r++;
			if (r == size_t(workshop::Role::Count))
				throw BaseException("matter: " + f.name + ": no machine role called " + machine.role);
			mat.role = workshop::Role(r);
		}
		mat.power = machine.power.value_or(0) * 1000;
		mat.rpm = machine.rpm.value_or(0);
		if (machine.tension) {
			mat.tension = *machine.tension;
			mat.compression = machine.compression.value_or(*machine.tension);
			mat.toughness = machine.toughness.value_or(0);
		} else if (const int y = itemgroup_get(f.groups, "pile_yield"); f.groups.count("pile_yield")) {
			// loose ground is held by being damp, whatever its grains are
			mat.tension = y == 0 ? 2e3f : 3e4f;
			mat.compression = y == 0 ? 5e4f : 3e5f;
			mat.toughness = 100;
		} else if (substance && substance->strength) {
			mat.tension = (float)(substance->strength->tension * share);
			mat.compression = (float)(substance->strength->compression * share);
			mat.toughness = (float)substance->strength->toughness;
		} else {
			for (const ByGroup &g : BY_GROUP) {
				const int level = itemgroup_get(f.groups, g.group);
				if (level <= 0)
					continue;
				const int i = std::clamp(level, 1, 3) - 1;
				mat.tension = g.tension[i];
				mat.compression = g.compression[i];
				mat.toughness = g.toughness[i];
				break;
			}
		}
		if (mat.tension <= 0 && mat.role == workshop::Role::None)
			unjudged.push_back(f.name);
		try {
			server->getMaterials().set(c, mat);
		} catch (const std::invalid_argument &e) {
			throw BaseException("matter: " + f.name + ": " + e.what());
		}
	}
	auto list = [](std::vector<std::string> names) {
		std::sort(names.begin(), names.end());
		std::string out;
		for (const std::string &n : names)
			out += (out.empty() ? "" : ", ") + n;
		return out;
	};
	if (!unjudged.empty())
		actionstream << "matter: " << unjudged.size() << " solid blocks have no strength, so they never fail: " << list(unjudged) << std::endl;
	if (!unweighed.empty())
		actionstream << "matter: " << unweighed.size() << " solid blocks have no matter, so a machine of them can't move: " << list(unweighed) << std::endl;
}

/*
	Running it (sprint 119: was tf_matter's globalstep, in Lua). Each period:
	the weather's temperature and wetness told to the 16-block cubes round each
	player; the awake blocks stepped; whoever stands in heat burned; what each
	player saw react recorded for the wiki; flames, smoke and sparks shown near
	players, with a flame node's light where air burns; and every minute, what's
	awake saved beside the world.
*/

namespace {

const float STEP = 0.5f;     // s between steps of the world's matter
const float SHOW = 0.5f;     // s between showing it near players
const float WEATHER = 5;     // s between telling it the weather
const float SAVE = 60;       // s between saves
const s16 REACH = 20;        // blocks round a player it's shown
const double HOT = 330;      // K: hot enough to hurt
const float SEEN = 16;       // blocks: close enough to have seen a reaction
const char *const FLAME = "core:flame";

// A particle spawner as Lua's legacy fields make one: positions and velocities in a box,
// a lifetime and size range, a texture fading (alpha) and growing or shrinking (scale).
ParticleSpawnerParameters spawner(u16 amount, v3f lo, v3f hi, v3f vlo, v3f vhi, float exp_lo, float exp_hi,
		float size_lo, float size_hi, const std::string &texture, float alpha_from, float alpha_to,
		float scale_from, float scale_to, u8 glow)
{
	using namespace ParticleParamTypes;
	ParticleSpawnerParameters p;
	p.amount = amount;
	p.time = SHOW;
	p.pos = v3fRangeTween(v3fRange(lo * BS, hi * BS));
	p.vel = v3fRangeTween(v3fRange(vlo * BS, vhi * BS));
	p.exptime = f32RangeTween(f32Range(exp_lo, exp_hi));
	p.size = f32RangeTween(f32Range(size_lo, size_hi));
	p.texture.string = texture;
	p.texture.alpha = f32Tween(alpha_from, alpha_to);
	p.texture.scale = v2fTween(v2f(scale_from), v2f(scale_to));
	p.glow = glow;
	return p;
}

std::vector<std::string> seenBy(PlayerSAO *sao)
{
	std::vector<std::string> ids;
	std::istringstream in(sao->getMeta().getString("tf_discovered"));
	for (std::string id; std::getline(in, id, ',');)
		if (!id.empty())
			ids.push_back(id);
	return ids;
}

void tellSeen(Server *server, PlayerSAO *sao, const std::vector<std::string> &ids)
{
	Json::Value v(Json::arrayValue);
	for (const std::string &id : ids)
		v.append(id);
	Json::StreamWriterBuilder writer;
	writer["indentation"] = "";
	server->setUi(sao->getPlayer(), "discovered", Json::writeString(writer, v));
}

std::vector<std::pair<RemotePlayer *, PlayerSAO *>> here(Server *server)
{
	std::vector<std::pair<RemotePlayer *, PlayerSAO *>> out;
	for (RemotePlayer *player : server->getEnv().getPlayers())
		if (PlayerSAO *sao = player->getPlayerSAO(); sao && player->getPeerId() != PEER_ID_INEXISTENT)
			out.push_back({ player, sao });
	return out;
}

} // namespace

void terraMatterDisturb(Server *server, v3s16 p)
{
	serverMatterStarted(server).world->disturb({ p.X, p.Y, p.Z });
}

void terraMatterJoin(Server *server, PlayerSAO *sao)
{
	tellSeen(server, sao, seenBy(sao));
}

void terraStepMatter(Server *server, float dtime)
{
	ServerMatter &m = serverMatterStarted(server);
	ServerMatter::Clock &clock = m.clock;
	clock.step += dtime;
	clock.show += dtime;
	clock.weather += dtime;
	clock.save += dtime;
	const auto players = here(server);

	if (clock.weather >= WEATHER) {
		clock.weather = 0;
		for (const auto &[player, sao] : players) {
			const v3s16 p = floatToInt(sao->getBasePosition(), BS);
			const std::optional<TerraWeather> w = terraWeather(server, intToFloat(p, 1));
			if (!w)
				continue; // (no weather where terra didn't make the world, on seam, or yet)
			for (s16 dx = -32; dx <= 32; dx += 16)
			for (s16 dy = -32; dy <= 32; dy += 16)
			for (s16 dz = -32; dz <= 32; dz += 16) {
				const int64_t c = matterCube({ p.X + dx, p.Y + dy, p.Z + dz });
				m.ambient[c] = w->sample.temperature;
				m.wetness[c] = std::clamp(w->sample.soil, 0.0, 1.0);
			}
		}
	}

	if (clock.step >= STEP) {
		m.world->step(clock.step);
		clock.step = 0;
		// standing in heat burns: by the hotter of the blocks at the feet and the head
		for (const auto &[player, sao] : players) {
			if (sao->getHP() == 0)
				continue;
			const v3s16 feet = floatToInt(sao->getBasePosition(), BS);
			const double t = std::max(m.world->temperature({ feet.X, feet.Y, feet.Z }),
					m.world->temperature({ feet.X, feet.Y + 1, feet.Z }));
			if (t > HOT)
				sao->setHP((s32)sao->getHP() - (s32)std::ceil((t - HOT) / 60), PlayerHPChangeReason(PlayerHPChangeReason::SET_HP));
		}
		// what each player saw react, for the wiki
		for (const auto &[p, reaction] : m.world->happened()) {
			const std::string &id = m.table.reactions()[reaction].id;
			for (const auto &[player, sao] : players) {
				if (sao->getBasePosition().getDistanceFrom(v3f(p.x, p.y, p.z) * BS) > SEEN * BS)
					continue;
				std::vector<std::string> seen = seenBy(sao);
				if (std::find(seen.begin(), seen.end(), id) != seen.end())
					continue;
				seen.push_back(id);
				std::sort(seen.begin(), seen.end());
				std::string joined;
				for (const std::string &s : seen)
					joined += (joined.empty() ? "" : ",") + s;
				sao->getMeta().setString("tf_discovered", joined);
				tellSeen(server, sao, seen);
			}
		}
	}

	if (clock.show >= SHOW) {
		clock.show = 0;
		// flames where air burns, smoke where gas isn't air, sparks off glowing solids
		std::unordered_map<v3s16, bool> burning;
		for (const auto &[player, sao] : players) {
			const v3s16 p = floatToInt(sao->getBasePosition(), BS);
			const v3s16 r(REACH, REACH, REACH);
			for (const WorldMatter::Awake &a : m.world->awake({ p.X - r.X, p.Y - r.Y, p.Z - r.Z }, { p.X + r.X, p.Y + r.Y, p.Z + r.Z })) {
				const v3f at(a.pos.x, a.pos.y, a.pos.z);
				if (a.activity > 50 && a.temperature > 700 && a.fill < 0.5) {
					burning[v3s16(a.pos.x, a.pos.y, a.pos.z)] = true;
					server->addParticleSpawner(spawner((u16)std::min(40.0, 8 + std::floor(a.activity / 1500)),
							at + v3f(-0.4f, -0.5f, -0.4f), at + v3f(0.4f, 0.1f, 0.4f), v3f(-0.1f, 0.8f, -0.1f), v3f(0.1f, 1.8f, 0.1f),
							0.25f, 0.6f, 0.8f, 2.2f, "matter_flame.png", 1, 0, 1, 0.3f, 14), nullptr, "", "");
				}
				if (a.vapour >= 0 && a.haze > 0.05 && m.table.substances()[a.vapour].id != "oxygen") {
					const bool steam = m.table.substances()[a.vapour].id == "water";
					server->addParticleSpawner(spawner((u16)std::ceil(a.haze * 12),
							at + v3f(-0.5f, 0, -0.5f), at + v3f(0.5f, 0.5f, 0.5f), v3f(-0.1f, 0.4f, -0.1f), v3f(0.1f, 1.0f, 0.1f),
							1.5f, 3, 2, 4.5f, steam ? "matter_steam.png" : "matter_smoke.png", 0.7f, 0, 0.6f, 1.6f, 0), nullptr, "", "");
				}
				if (a.fill >= 0.5 && a.temperature > 900)
					server->addParticleSpawner(spawner(3, at + v3f(-0.5f, 0.5f, -0.5f), at + v3f(0.5f, 0.5f, 0.5f),
							v3f(-0.3f, 1, -0.3f), v3f(0.3f, 2, 0.3f), 0.5f, 1.2f, 0.3f, 0.6f, "matter_spark.png", 1, 0, 1, 1, 14),
							nullptr, "", "");
			}
		}
		// a flame's light: the flame node where air burns, back to air where it stops
		const NodeDefManager *ndef = server->ndef();
		const content_t flame = ndef->getId(FLAME);
		if (flame != CONTENT_IGNORE) {
			ServerEnvironment &env = server->getEnv();
			for (const auto &[p, _] : burning)
				if (!m.lit.count(p) && env.getMap().getNode(p).getContent() == CONTENT_AIR) {
					env.swapNode(p, MapNode(flame));
					m.lit.insert(p);
				}
			for (auto it = m.lit.begin(); it != m.lit.end();) {
				if (burning.count(*it)) {
					++it;
					continue;
				}
				if (env.getMap().getNode(*it).getContent() == flame)
					env.swapNode(*it, MapNode(CONTENT_AIR));
				it = m.lit.erase(it);
			}
		}
	}

	if (clock.save >= SAVE) {
		clock.save = 0;
		serverMatterSave();
	}
}

void terraFillShore(Server *server, const std::vector<v3s16> &shore)
{
	if (shore.empty())
		return;
	const matter::Matter &table = serverMatter(server).table;
	const matter::Substance &water = table.substances()[table.index("water")];
	fill::Fill f;
	f.liquid = fill::liquid(water.id);
	f.amount = 255;
	f.r = (water.colour >> 16) & 0xFF;
	f.g = (water.colour >> 8) & 0xFF;
	f.b = water.colour & 0xFF;
	Map &map = server->getEnv().getMap();
	// only where it's open (smoothed): a whole block of sand under the sea holds none
	for (const v3s16 &p : shore)
		if (matterRoomAt(map, server->ndef(), p).total() > 0)
			map.setFill(p, f);
}

Json::Value serverMatterBaked(const NodeDefManager *ndef)
{
	ServerMatter *matter = serverMatterIfMade();
	if (!matter)
		throw BaseException("export: the game's matter never started");
	Json::Value out(Json::arrayValue);
	for (const sandbox::WorldMatter::Spec &s : matter->plain) {
		const ContentFeatures &f = ndef->get(s.node);
		Json::Value b;
		b["node"] = s.node;
		b["matter"] = s.matter;
		b["share"] = s.share;
		b["liquid"] = s.state == matter::State::Liquid;
		b["form"] = s.form;
		b["flowing"] = s.flowing;
		b["vessel"] = s.vessel;
		b["leveled"] = s.leveled;
		b["loose"] = s.loose;
		// (top, bottom, side: as tiledef's order has them)
		for (int t : { 0, 1, 2 })
			b["tiles"].append(f.tiledef[t].name);
		if (f.node_box.type == NODEBOX_FIXED)
			for (const aabb3f &box : f.node_box.fixed) {
				Json::Value part(Json::arrayValue);
				for (float v : { box.MinEdge.X, box.MinEdge.Y, box.MinEdge.Z, box.MaxEdge.X, box.MaxEdge.Y, box.MaxEdge.Z })
					part.append(v / BS + 0.5f); // from the block's corner
				b["boxes"].append(part);
			}
		out.append(b);
	}
	return out;
}
