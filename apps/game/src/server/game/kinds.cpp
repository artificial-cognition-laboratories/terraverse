// Terra Firma: what an entity does, by its name (sprint 119). See kinds.h. The item and
// falling block were builtin/game/item_entity.lua and falling.lua; the machine's carrier
// was builtin/game/world.lua's core:piece.
#include "src/server/game/kinds.h"

#include "src/shared/game/objects/collision.h"
#include "src/shared/base/exceptions.h"
#include "src/server/game/interact.h"
#include "src/shared/game/items/inventory.h"
#include "src/shared/game/items/itemdef.h"
#include "src/shared/platform/system/log.h"
#include "src/server/world/state/objects/entity_sao.h"
#include "src/shared/map/map.h"
#include "src/shared/game/content/nodedef.h"
#include "library/world/planet/atlas.h"
#include "src/server/players/player_sao.h"
#include "src/shared/game/objects/remoteplayer.h"
#include "src/server/app/server.h"
#include "src/server/world/state/environment/serverenvironment.h"
#include "src/shared/platform/settings/settings.h"
#include <json/json.h>
#include <cmath>
#include <map>
#include <sstream>

namespace {

const char ITEM[] = "__builtin:item";
const char FALLING[] = "__builtin:falling_node";

float gravity()
{
	return g_settings->getFloat("movement_gravity") * BS;
}

Json::Value parse(const std::string &state)
{
	Json::Value v;
	std::istringstream in(state);
	std::string errors;
	if (!Json::parseFromStream(Json::CharReaderBuilder(), in, &v, &errors))
		return Json::Value();
	return v;
}

std::string write(const Json::Value &v)
{
	Json::StreamWriterBuilder w;
	w["indentation"] = "";
	return Json::writeString(w, v);
}

// What a world saved before (Lua's serialize: return { ["key"] = "value", … }): the text
// of one string field, or "" if it's not there.
std::string luaField(const std::string &state, const std::string &key)
{
	const std::string tag = "[\"" + key + "\"] = \"";
	size_t i = state.find(tag);
	if (i == std::string::npos)
		return "";
	std::string out;
	for (i += tag.size(); i < state.size() && state[i] != '"'; i++) {
		if (state[i] == '\\' && i + 1 < state.size())
			i++;
		out += state[i];
	}
	return out;
}

// Something dropped, lying on the ground to be picked up (a punch takes it), merging with
// others of its kind that lie close. Gone after item_entity_ttl seconds.
class Item : public EntityKind
{
public:
	void activate(EntitySAO &self, const std::string &state, u32 dtime_s) override
	{
		ObjectProperties &p = *self.accessObjectProperties();
		p.hp_max = 1;
		p.physical = true;
		p.collideWithObjects = false;
		p.collisionbox = p.selectionbox = aabb3f(-0.3f, -0.3f, -0.3f, 0.3f, 0.3f, 0.3f);
		p.visual = OBJECTVISUAL_WIELDITEM;
		p.visual_size = v3f(0.4f, 0.4f, 0.4f);
		p.textures = { "" };
		p.is_visible = false;

		if (state.rfind("return", 0) == 0) { // saved by Lua
			m_item = luaField(state, "itemstring");
		} else if (const Json::Value v = parse(state); v.isObject()) {
			m_item = v["item"].asString();
			m_age = v["age"].asFloat();
		} else {
			m_item = state; // only the item, as Lua's oldest
		}
		m_age += dtime_s;
		self.setArmorGroups({ { "immortal", 1 } });
		self.setVelocity(v3f(0, 2 * BS, 0));
		self.setAcceleration(v3f(0, -gravity(), 0));
		set(self, m_item);
	}

	std::string state(const EntitySAO &) const override
	{
		Json::Value v;
		v["item"] = m_item;
		v["age"] = m_age;
		return write(v);
	}

	void step(EntitySAO &self, float dtime, const CollisionMoveResult *moved) override
	{
		ServerEnvironment &env = *self.getEnv();
		m_age += dtime;
		const float ttl = g_settings->getFloat("item_entity_ttl");
		if (ttl > 0 && m_age > ttl) {
			remove(self);
			return;
		}
		const v3f pos = self.getBasePosition() / BS;
		// gone where there's no map (ignore)
		if (env.getMap().getNode(floatToInt(v3f(pos.X, pos.Y + m_box.MinEdge.Y - 0.05f, pos.Z), 1)).getContent() == CONTENT_IGNORE) {
			remove(self);
			return;
		}
		if (m_force_out) {
			// pushed out of a solid block: until it's clear of it
			const aabb3f &c = m_box;
			const v3f &f = *m_force_out;
			const v3f s = intToFloat(m_force_out_start, 1);
			const bool out = (f.X > 0 && pos.X + c.MinEdge.X > s.X + 0.5f) ||
					(f.Y > 0 && pos.Y + c.MinEdge.Y > s.Y + 0.5f) ||
					(f.Z > 0 && pos.Z + c.MinEdge.Z > s.Z + 0.5f) ||
					(f.X < 0 && pos.X + c.MaxEdge.X < s.X - 0.5f) ||
					(f.Z < 0 && pos.Z + c.MaxEdge.Z < s.Z - 0.5f);
			if (out) {
				m_force_out.reset();
				physics(self, true);
				return;
			}
		}
		if (!m_physical || !moved || !moved->collides)
			return;

		const NodeDefManager *ndef = env.getGameDef()->ndef();
		// stuck in a solid block: out by whichever side is free, or up
		const v3s16 here = floatToInt(pos, 1);
		bool loaded;
		const MapNode in = env.getMap().getNode(here, &loaded);
		if (loaded) {
			const ContentFeatures &f = ndef->get(in);
			if (f.walkable && f.collision_box.type == NODEBOX_REGULAR && f.node_box.type == NODEBOX_REGULAR) {
				static const v3s16 SIDES[4] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
				std::optional<v3s16> dir;
				for (const v3s16 &d : SIDES) {
					const MapNode side = env.getMap().getNode(here + d);
					if (side.getContent() != CONTENT_IGNORE && !ndef->get(side).walkable) {
						dir = d;
						break;
					}
				}
				if (!dir && env.getMap().getNode(here + v3s16(0, 1, 0)).getContent() != CONTENT_IGNORE)
					dir = v3s16(0, 1, 0);
				if (dir) {
					physics(self, false);
					m_force_out = intToFloat(*dir, 1) * 3;
					m_force_out_start = here;
					self.setVelocity(*m_force_out * BS);
					return;
				}
			}
		}

		// on the ground: slides on what's slippery, else stops
		bool sliding = false;
		if (moved->touching_ground) {
			for (const CollisionInfo &c : moved->collisions) {
				if (c.axis != COLLISION_AXIS_Y || c.type != COLLISION_NODE)
					continue;
				const int slippery = itemgroup_get(ndef->get(env.getMap().getNode(c.node_p)).groups, "slippery");
				const v3f vel = self.getVelocity() / BS;
				if (slippery != 0 && (std::fabs(vel.X) > 0.1f || std::fabs(vel.Z) > 0.1f)) {
					const float k = std::min(4.0f / (slippery + 4) * dtime, 1.0f);
					self.setVelocity(v3f(vel.X * (1 - k), 0, vel.Z * (1 - k)) * BS);
					sliding = true;
				}
				break;
			}
		}
		if (!sliding)
			self.setVelocity(v3f(0, 0, 0));
		if (m_moving == sliding)
			return; // only when it comes to rest
		m_moving = sliding;
		if (m_moving)
			return;
		merge(self);
	}

	bool punch(EntitySAO &self, ServerActiveObject *puncher) override
	{
		if (m_item.empty()) {
			self.markForRemoval();
			return true;
		}
		auto *player = dynamic_cast<PlayerSAO *>(puncher);
		Inventory *inv = player ? player->getInventory() : nullptr;
		InventoryList *main = inv ? inv->getList("main") : nullptr;
		if (!main)
			return true;
		ServerEnvironment &env = *self.getEnv();
		IItemDefManager *idef = env.getGameDef()->getItemDefManager();
		ItemStack stack;
		stack.deSerialize(m_item, idef);
		const ItemStack left = main->addItem(stack);
		if (left.count != stack.count) {
			env.getServer()->SendInventory(player->getPlayer(), true);
			actionstream << player->getPlayer()->getName() << " picks up " << stack.getItemString(false) << std::endl;
		}
		if (left.empty())
			remove(self);
		else
			set(self, left.getItemString());
		return true;
	}

	const std::string &item() const { return m_item; }
	float age() const { return m_age; }

	void set(EntitySAO &self, const std::string &item)
	{
		m_item = item;
		if (m_item.empty())
			return;
		ServerEnvironment &env = *self.getEnv();
		const IItemDefManager *idef = env.getGameDef()->idef();
		ItemStack stack;
		stack.deSerialize(m_item, env.getGameDef()->getItemDefManager());
		const std::string name = stack.isKnown(idef) ? stack.name : "unknown";
		const u16 max = stack.getStackMax(idef);
		const float size = 0.2f + 0.1f * std::cbrt((float)std::min(stack.count, max) / max);
		ObjectProperties &p = *self.accessObjectProperties();
		const float bias = 1e-3f * (float)(self.getId() % 97) / 97; // against z-fighting
		m_box = aabb3f(-size, -size, -size, size, size, size);
		p.is_visible = true;
		p.visual = OBJECTVISUAL_WIELDITEM;
		p.textures = { name };
		p.visual_size = v3f(size + bias, size + bias, size + bias);
		p.collisionbox = p.selectionbox = m_box;
		p.automatic_rotate = M_PI * 0.5f * 0.2f / size;
		p.wield_item = m_item;
		const u8 light = name == "unknown" ? 0 : env.getGameDef()->ndef()->get(name).light_source;
		p.glow = (s8)std::floor(light / 2.0f + 0.5f);
		p.infotext = stack.getDescription(idef);
		self.notifyObjectPropertiesModified();
	}

private:
	void remove(EntitySAO &self)
	{
		m_item.clear();
		self.markForRemoval();
	}

	void physics(EntitySAO &self, bool on)
	{
		if (m_physical == on)
			return;
		m_physical = on;
		self.accessObjectProperties()->physical = on;
		self.notifyObjectPropertiesModified();
		self.setVelocity(v3f(0, 0, 0));
		self.setAcceleration(on ? v3f(0, -gravity(), 0) : v3f(0, 0, 0));
	}

	// Others of its kind lying within a node join it while there's room in the stack.
	void merge(EntitySAO &self)
	{
		ServerEnvironment &env = *self.getEnv();
		IItemDefManager *idef = env.getGameDef()->getItemDefManager();
		ItemStack own;
		own.deSerialize(m_item, idef);
		if (own.freeSpace(idef) == 0)
			return;
		std::vector<ServerActiveObject *> near;
		env.getObjectsInsideRadius(near, self.getBasePosition(), 1.0f * BS, [&](ServerActiveObject *obj) {
			return obj != &self && !obj->isGone() && obj->getType() == ACTIVEOBJECT_TYPE_LUAENTITY;
		});
		for (ServerActiveObject *obj : near) {
			auto *other = static_cast<EntitySAO *>(obj);
			auto *it = dynamic_cast<Item *>(other->kind());
			if (!it || it->m_item.empty())
				continue;
			ItemStack theirs;
			theirs.deSerialize(it->m_item, idef);
			if (!own.stacksWith(theirs) || own.count + theirs.count > own.getStackMax(idef))
				continue;
			v3f at = other->getBasePosition();
			at.Y += (float)theirs.count / own.getStackMax(idef) * 0.15f * BS;
			self.moveTo(at, false);
			m_age = 0; // as if new
			own.count += theirs.count;
			set(self, own.getItemString());
			it->remove(*other);
			if (own.freeSpace(idef) == 0)
				return;
		}
	}

	std::string m_item;
	float m_age = 0;
	bool m_moving = true, m_physical = true;
	aabb3f m_box = aabb3f(-0.3f, -0.3f, -0.3f, 0.3f, 0.3f, 0.3f);
	std::optional<v3f> m_force_out;
	v3s16 m_force_out_start;
};

// A block falling. It lands as the block where it comes to rest, or as items when it
// can't (it lands on the edge of something, or on what won't give way).
class Falling : public EntityKind
{
public:
	void activate(EntitySAO &self, const std::string &state, u32) override
	{
		ObjectProperties &p = *self.accessObjectProperties();
		p.visual = OBJECTVISUAL_NODE;
		p.physical = true;
		p.is_visible = false;
		p.collideWithObjects = true;
		p.collisionbox = aabb3f(-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f);
		self.setArmorGroups({ { "immortal", 1 } });
		self.setAcceleration(v3f(0, -gravity(), 0));

		const NodeDefManager *ndef = self.getEnv()->getGameDef()->ndef();
		std::string name;
		u8 param2 = 0;
		if (state.rfind("return", 0) == 0) { // saved by Lua: its name alone is kept
			name = luaField(state, "name");
		} else if (const Json::Value v = parse(state); v.isObject()) {
			name = v["name"].asString();
			param2 = (u8)v["param2"].asUInt();
			m_level = v["level"].asInt();
		}
		content_t c;
		if (name.empty() || !ndef->getId(name, c)) {
			infostream << "falling: an unknown block (" << name << ") goes" << std::endl;
			self.markForRemoval();
			return;
		}
		set(self, MapNode(c, 0, param2), m_level);
	}

	std::string state(const EntitySAO &self) const override
	{
		Json::Value v;
		v["name"] = m_name;
		v["param2"] = m_node.param2;
		v["level"] = m_level;
		return write(v);
	}

	void set(EntitySAO &self, MapNode n, int level)
	{
		m_node = n;
		m_level = level;
		const ContentFeatures &f = self.getEnv()->getGameDef()->ndef()->get(n);
		m_name = f.name;
		m_floats = itemgroup_get(f.groups, "float") != 0;
		ObjectProperties &p = *self.accessObjectProperties();
		if (f.drawtype != NDT_AIRLIKE) {
			p.is_visible = true;
			p.node = n;
			p.glow = f.light_source;
		}
		// shaped like it, where it's one box
		if (f.drawtype == NDT_NODEBOX && f.node_box.fixed.size() == 1 &&
				(f.node_box.type == NODEBOX_FIXED || f.node_box.type == NODEBOX_LEVELED)) {
			aabb3f box = f.node_box.fixed[0];
			box.MinEdge /= BS;
			box.MaxEdge /= BS;
			if (f.param_type_2 == CPT2_LEVELED && level > 0)
				box.MaxEdge.Y = -0.5f + level / 64.0f;
			p.collisionbox = box;
		}
		self.notifyObjectPropertiesModified();
	}

	void step(EntitySAO &self, float, const CollisionMoveResult *moved) override
	{
		ServerEnvironment &env = *self.getEnv();
		const NodeDefManager *ndef = env.getGameDef()->ndef();
		const v3f pos = self.getBasePosition() / BS;
		// liquids don't collide: one that floats lands on them by looking
		if (m_floats) {
			const v3s16 below = floatToInt(pos + v3f(0, -0.7f, 0), 1);
			const MapNode bn = env.getMap().getNode(below);
			if (ndef->get(bn).liquid_type != LIQUID_NONE && land(env, below, bn)) {
				self.markForRemoval();
				return;
			}
		}
		if (!moved || !moved->collides)
			return;

		std::optional<v3s16> on;
		const CollisionInfo *player = nullptr;
		if (moved->touching_ground) {
			for (const CollisionInfo &c : moved->collisions) {
				if (c.type == COLLISION_OBJECT) {
					if (c.axis == COLLISION_AXIS_Y && c.object && c.object->getType() == ACTIVEOBJECT_TYPE_PLAYER)
						player = &c;
				} else if (c.axis == COLLISION_AXIS_Y) {
					on = c.node_p;
					break;
				}
			}
		}
		if (!on) {
			// on a player's head: falls on through them
			if (player) {
				const v3f vel = self.getVelocity();
				self.setVelocity(v3f(vel.X, player->old_speed.Y, vel.Z));
				self.setPos(self.getBasePosition() + v3f(0, -0.5f * BS, 0));
			}
			return;
		}
		MapNode bn = env.getMap().getNode(*on);
		if (bn.getContent() == CONTENT_IGNORE) {
			self.markForRemoval(); // the edge of the world
			return;
		}
		bool failed = false;
		const v3f d = apart(env, pos, *on);
		if (std::fabs(d.X) >= 1 || std::fabs(d.Z) >= 1) {
			failed = true; // on the edge of something sticking out: it'd jump to land
		} else if (std::fabs(d.Y) >= 2) {
			// a tall block's hidden top (a door's): check what's there
			*on += v3s16(0, 1, 0);
			bn = env.getMap().getNode(*on);
			if (!ndef->get(bn).walkable)
				failed = true;
		}
		if (!failed)
			failed = !land(env, *on, bn);
		if (failed)
			for (const ItemStack &item : interact::drops(env.getServer(), m_node))
				spawnItem(env, pos, item);
		self.markForRemoval();
	}

private:
	// From `pos` to the block `to`, along pos's grid's axes; on an atlas planet the block
	// may be across a seam, stored in another grid far off, so measured on the planet.
	static v3f apart(ServerEnvironment &env, v3f pos, v3s16 to)
	{
		const planet::Atlas *atlas = env.getServer()->atlas();
		const auto point = [&](v3f q) {
			const v3s16 n = floatToInt(q, 1);
			return atlas ? atlas->point({ n.X, n.Y, n.Z }, q.X, q.Y, q.Z) : std::nullopt;
		};
		const auto from = point(pos);
		if (!from)
			return intToFloat(to, 1) - pos; // one grid
		const auto there = point(intToFloat(to, 1));
		if (!there)
			return intToFloat(to, 1) - pos;
		const v3d dd(there->x - from->x, there->y - from->y, there->z - from->z);
		const double h = 0.01;
		const auto along = [&](v3f axis) {
			const auto q = point(pos + axis * h);
			if (!q)
				return 0.0;
			const v3d e((q->x - from->x) / h, (q->y - from->y) / h, (q->z - from->z) / h);
			return dd.dotProduct(e) / e.dotProduct(e);
		};
		return v3f(along(v3f(1, 0, 0)), along(v3f(0, 1, 0)), along(v3f(0, 0, 1)));
	}

	// Lands on (or into) the block at `at`. False if it can't.
	bool land(ServerEnvironment &env, v3s16 at, MapNode bn)
	{
		const NodeDefManager *ndef = env.getGameDef()->ndef();
		const ContentFeatures &fb = ndef->get(bn), &f = ndef->get(m_node);
		// onto one of its own levelled kind: it fills it
		if (fb.param_type_2 == CPT2_LEVELED && bn.getContent() == m_node.getContent()) {
			const s16 add = m_level > 0 ? m_level : fb.leveled;
			const s8 rest = bn.addLevel(ndef, add);
			env.setNode(at, bn);
			if (rest < add || fb.buildable_to)
				return true;
		}
		v3s16 into = at;
		if (fb.buildable_to && (!m_floats || fb.liquid_type == LIQUID_NONE ||
				(f.liquid_type != LIQUID_NONE && fb.liquid_type != LIQUID_SOURCE))) {
			env.removeNode(at);
		} else {
			into += v3s16(0, 1, 0);
			const MapNode above = env.getMap().getNode(into);
			const ContentFeatures &fa = ndef->get(above);
			if (!fa.buildable_to) {
				// 'walkable' here: what a falling block can't replace
				if (!fa.diggable || fa.walkable)
					return false;
				interact::dig(env.getServer(), nullptr, into);
				if (env.getMap().getNode(into).getContent() == above.getContent())
					return false;
			} else {
				env.removeNode(into);
			}
		}
		MapNode n = m_node;
		if (f.param_type_2 == CPT2_LEVELED && m_level > 0)
			n.setLevel(ndef, m_level);
		env.setNode(into, n);
		interact::settle(env, into);
		return true;
	}

	MapNode m_node = MapNode(CONTENT_AIR);
	std::string m_name; // its block's, as it's saved
	int m_level = 0;
	bool m_floats = false;
};

// A machine off the world's grid (sprint 49): the engine makes, holds, edits and moves it
// (src/workshop); this only carries it, its text kept as its state. Physical, so players
// stand on it and bump into it.
class Piece : public EntityKind
{
public:
	void activate(EntitySAO &self, const std::string &state, u32) override
	{
		if (state.empty())
			throw BaseException("core:piece: made without its machine");
		ObjectProperties &p = *self.accessObjectProperties();
		p.visual = OBJECTVISUAL_PIECE;
		p.physical = true;
		p.collideWithObjects = true;
		p.pointable = PointabilityType::POINTABLE;
		p.static_save = true;
		p.piece = state;
		self.setArmorGroups({ { "immortal", 1 } });
	}

	std::string state(const EntitySAO &self) const override
	{
		return self.properties().piece;
	}
};

} // namespace

static std::map<std::string, std::function<std::unique_ptr<EntityKind>()>> &probeKinds()
{
	static std::map<std::string, std::function<std::unique_ptr<EntityKind>()>> kinds;
	return kinds;
}

void registerProbeKind(const std::string &name, std::function<std::unique_ptr<EntityKind>()> make)
{
	probeKinds()[name] = std::move(make);
}

std::unique_ptr<EntityKind> entityKind(const std::string &name)
{
	if (auto it = probeKinds().find(name); it != probeKinds().end())
		return it->second();
	if (name == ITEM)
		return std::make_unique<Item>();
	if (name == FALLING)
		return std::make_unique<Falling>();
	if (name == "core:piece" || name == "tf_world:piece") // (machines saved before)
		return std::make_unique<Piece>();
	return nullptr;
}

EntitySAO *spawnItem(ServerEnvironment &env, v3f pos, const ItemStack &stack)
{
	if (stack.empty())
		return nullptr;
	auto obj = std::make_unique<EntitySAO>(&env, pos * BS, ITEM, "");
	EntitySAO *out = obj.get();
	if (env.addActiveObject(std::move(obj)) == 0)
		return nullptr;
	auto *item = dynamic_cast<Item *>(out->kind());
	if (!item)
		throw BaseException("spawnItem: the item entity isn't an item");
	item->set(*out, stack.getItemString());
	return out;
}

bool spawnFalling(ServerEnvironment &env, v3s16 p, MapNode n)
{
	const NodeDefManager *ndef = env.getGameDef()->ndef();
	const int level = ndef->get(n).param_type_2 == CPT2_LEVELED ? n.getLevel(ndef) : 0;
	Json::Value v;
	v["name"] = ndef->get(n).name;
	v["param2"] = n.param2;
	v["level"] = level;
	auto obj = std::make_unique<EntitySAO>(&env, intToFloat(p, BS), FALLING, write(v));
	if (env.addActiveObject(std::move(obj)) == 0)
		return false;
	env.removeNode(p);
	return true;
}
