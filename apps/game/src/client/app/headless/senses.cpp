// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/headless/senses.h"
#include "src/client/app/headless/rig.h"
#include "src/client/world/objects/content_cao.h"
#include "src/client/world/player/figure.h"
#include "src/client/world/objects/localplayer.h"
#include "src/client/world/player/body.h"
#include "src/client/world/session/client.h"
#include "src/client/world/session/clientenvironment.h"
#include "src/shared/base/constants.h"
#include "src/shared/game/content/nodedef.h"
#include "src/shared/map/map.h"
#include "src/shared/util/text/string.h"
#include <cmath>

namespace headless {

namespace {

constexpr float RAD = 0.01745329f;
// How far round the body /blocks reads, nodes: a cube 2r+1 wide.
constexpr int REACH = 3;

// The engine's axes (x east, y up, z north, BS a node) to the world place's (x east, y north, z up, m).
v3f placed(v3f engine) { return v3f(engine.X, engine.Z, engine.Y) / BS; }

// The head's axes in the world place, looking `yaw` (+ left of north) and `pitch` (+ down), degrees.
struct Axes
{
	v3f forward, left, up;
};
Axes headAxes(float yaw, float pitch);
v3f inHead(const Axes &a, v3f world) { return v3f(world.dotProduct(a.forward), world.dotProduct(a.left), world.dotProduct(a.up)); }
Json::Value values(std::initializer_list<float> v);
Json::Value vector(const char *channel, const char *frame, const char *profile, std::initializer_list<const char *> names, const char *unit);

} // namespace

Senses::Senses(Opts o) :
	m_client(o.client)
{}

Json::Value Senses::declared()
{
	Json::Value list(Json::arrayValue);
	Json::Value imu = vector("/imu", "head", "sensor_msgs/Imu", { "accel.x", "accel.y", "accel.z" }, "m/s²");
	for (const char *name : { "gyro.x", "gyro.y", "gyro.z" }) {
		Json::Value c;
		c["name"] = name;
		c["unit"] = "rad/s";
		imu["components"].append(c);
	}
	imu["rate"] = 20;
	list.append(imu);
	Json::Value contact = vector("/contact", "base", "axon/contact", { "ground", "climbing", "submerged" }, "bool");
	contact["rate"] = 20;
	list.append(contact);
	Json::Value intero = vector("/interoception", nullptr, "axon/interoception", { "health", "breath" }, "fraction");
	Json::Value dead;
	dead["name"] = "dead";
	dead["unit"] = "bool";
	intero["components"].append(dead);
	intero["rate"] = 4;
	list.append(intero);
	Json::Value position = vector("/position", "world", "axon/position", { "x", "y", "z" }, "m");
	for (const char *name : { "yaw", "pitch" }) {
		Json::Value c;
		c["name"] = name;
		c["unit"] = "rad";
		position["components"].append(c);
	}
	position["rate"] = 20;
	list.append(position);
	Json::Value blocks;
	blocks["channel"] = "/blocks";
	blocks["kind"] = "text";
	blocks["format"] = "json";
	blocks["frame"] = "world";
	blocks["profile"] = "terraverse/blocks";
	blocks["rate"] = 2;
	blocks["schema"]["type"] = "object";
	blocks["schema"]["description"] = "The blocks within " + std::to_string(REACH) + " of the body that aren't air: "
		"`at` is the body's own node, in the world place (m); each block [dx, dy, dz, name] from it";
	list.append(blocks);
	Json::Value chat;
	chat["channel"] = "/chat";
	chat["kind"] = "text";
	chat["format"] = "json";
	chat["schema"]["type"] = "object";
	chat["schema"]["description"] = "A line said in the world: { from, text }; from is empty for the server's own";
	list.append(chat);
	list.append(rig::sense());
	return list;
}

bool Senses::due(Due &d, unsigned long long now)
{
	if (now < d.next)
		return false;
	d.next = now + d.every;
	return true;
}

std::vector<Json::Value> Senses::read(float dtime, unsigned long long now, float yaw, float pitch)
{
	std::vector<Json::Value> out;
	LocalPlayer *player = m_client.getEnv().getLocalPlayer();
	auto reading = [&](const char *channel) {
		Json::Value r;
		r["ch"] = channel;
		r["at"] = Json::UInt64(now);
		return r;
	};

	const v3f speed = placed(player->getSpeed());
	const v3f foot = player->getPosition() / BS;
	// the world's pull, in the place's axes (a field on a planet); none till its bodies are known
	const std::optional<v3f> pull = m_client.getBody().gravityAt(v3d(foot.X, foot.Y, foot.Z));
	if (pull && due(m_imu, now) && dtime > 0) {
		const Axes axes = headAxes(yaw, pitch);
		// what an accelerometer reads: the acceleration less gravity's pull (so at rest, 1 g up)
		const v3f accel = m_moved ? (speed - m_last_speed) / dtime : v3f(0);
		const v3f force = accel - v3f(pull->X, pull->Z, pull->Y);
		const v3f turned = v3f(0, 0, (yaw - m_last_yaw) * RAD / dtime) + axes.left * ((pitch - m_last_pitch) * RAD / dtime);
		const v3f f = inHead(axes, force), w = inHead(axes, turned);
		Json::Value r = reading("/imu");
		r["v"] = values({ f.X, f.Y, f.Z, w.X, w.Y, w.Z });
		out.push_back(r);
	}
	m_last_speed = speed;
	m_last_yaw = yaw;
	m_last_pitch = pitch;
	m_moved = true;

	if (due(m_contact, now)) {
		Json::Value r = reading("/contact");
		r["v"] = values({ player->touching_ground ? 1.f : 0.f, player->is_climbing ? 1.f : 0.f, player->in_liquid ? 1.f : 0.f });
		out.push_back(r);
	}
	if (due(m_interoception, now)) {
		Json::Value r = reading("/interoception");
		r["v"] = values({ std::min(1.f, player->hp / (float)PLAYER_MAX_HP_DEFAULT),
			std::min(1.f, player->getBreath() / (float)PLAYER_MAX_BREATH_DEFAULT), player->isDead() ? 1.f : 0.f });
		out.push_back(r);
	}
	if (due(m_position, now)) {
		const v3f at = placed(player->getPosition());
		Json::Value r = reading("/position");
		r["v"] = values({ at.X, at.Y, at.Z, yaw * RAD, pitch * RAD });
		out.push_back(r);
	}
	if (due(m_blocks, now)) {
		const v3s16 here = floatToInt(player->getPosition(), BS);
		const NodeDefManager *ndef = m_client.getNodeDefManager();
		Map &map = m_client.getEnv().getMap();
		Json::Value seen;
		const v3f at = placed(intToFloat(here, BS));
		seen["at"] = values({ at.X, at.Y, at.Z });
		seen["blocks"] = Json::Value(Json::arrayValue);
		for (s16 dx = -REACH; dx <= REACH; dx++)
		for (s16 dy = -REACH; dy <= REACH; dy++)
		for (s16 dz = -REACH; dz <= REACH; dz++) {
			bool valid = false;
			const MapNode n = map.getNodeSeen(here, here + v3s16(dx, dy, dz), &valid);
			if (!valid || n.getContent() == CONTENT_AIR || n.getContent() == CONTENT_IGNORE)
				continue;
			Json::Value b(Json::arrayValue);
			// in the world place's axes: east, north, up
			b.append(dx);
			b.append(dz);
			b.append(dy);
			b.append(ndef->get(n).name);
			seen["blocks"].append(b);
		}
		Json::Value r = reading("/blocks");
		r["text"] = seen;
		out.push_back(r);
	}
	// how the figure stands: its joints, once the game has posed it
	const GenericCAO *cao = player->getCAO();
	const Figure *figure = cao ? cao->figure() : nullptr;
	if (figure && due(m_joints, now)) {
		Json::Value r = reading("/joints");
		r["v"] = Json::Value(Json::arrayValue);
		for (float a : rig::angles(figure->bones()))
			r["v"].append(a);
		out.push_back(r);
	}
	while (std::optional<ChatMessage> said = m_client.takeChatMessage()) {
		Json::Value line;
		line["from"] = wide_to_utf8(said->sender);
		line["text"] = wide_to_utf8(said->message);
		Json::Value r = reading("/chat");
		r["text"] = line;
		out.push_back(r);
	}
	return out;
}

namespace {

Axes headAxes(float yaw, float pitch)
{
	const float y = yaw * RAD, p = pitch * RAD;
	Axes a;
	// yaw 0 looks north; + turns it west (left)
	a.forward = v3f(-std::sin(y) * std::cos(p), std::cos(y) * std::cos(p), -std::sin(p));
	a.left = v3f(-std::cos(y), -std::sin(y), 0);
	a.up = a.forward.crossProduct(a.left);
	return a;
}

Json::Value values(std::initializer_list<float> v)
{
	Json::Value list(Json::arrayValue);
	for (float x : v)
		list.append(x);
	return list;
}

Json::Value vector(const char *channel, const char *frame, const char *profile, std::initializer_list<const char *> names, const char *unit)
{
	Json::Value sense;
	sense["channel"] = channel;
	sense["kind"] = "vector";
	if (frame)
		sense["frame"] = frame;
	sense["profile"] = profile;
	for (const char *name : names) {
		Json::Value c;
		c["name"] = name;
		c["unit"] = unit;
		sense["components"].append(c);
	}
	return sense;
}

} // namespace

} // namespace headless
