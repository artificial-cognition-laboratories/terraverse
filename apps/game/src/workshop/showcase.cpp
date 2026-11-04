// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "showcase.h"

#include "constants.h"
#include "exceptions.h"
#include "gamedef.h"
#include "motion.h"
#include "nodedef.h"
#include "piece/machine.h"
#include "starters.h"

namespace workshop {

namespace {

content_t idOf(IGameDef *gamedef, const char *name)
{
	content_t id;
	if (!gamedef->ndef()->getId(name, id))
		throw BaseException(std::string("showcase: the game has no ") + name);
	return id;
}

// A deck two nodes by four on the starter corners, seat and engine on it: front steers, back drives.
piece::Machine car(IGameDef *gamedef)
{
	constexpr s16 WIDE = 2, LONG = 4;
	const content_t timber = idOf(gamedef, "tf_world:timber");
	piece::Object deck(gamedef);
	shape::Shape plate; // the corners' hubs meet it at their middles; the fittings stand on top
	for (s16 x = 0; x < 16; x++)
		for (s16 y = 5; y < 16; y++)
			for (s16 z = 0; z < 16; z++)
				plate.set({ x, y, z }, y >= 14 || x < 2 || x >= 14);
	for (s16 x = 0; x < WIDE; x++)
		for (s16 z = 0; z < LONG; z++) {
			deck.design().map().setNode({ x, 0, z }, MapNode(timber));
			deck.design().map().setShape({ x, 0, z }, plate);
		}
	deck.design().map().setNode({ 0, 1, 1 }, MapNode(idOf(gamedef, "tf_world:seat")));
	deck.design().map().setNode({ 1, 1, 2 }, MapNode(idOf(gamedef, "tf_world:motor")));
	deck.build();
	piece::Machine m(gamedef);
	const u64 body = m.add({ std::move(deck), piece::Pose{ v3s32(0), 0 } }).object.id();
	m.setFront(v3s16(0, 0, 1));
	const auto corner = [&](const char *name, s16 x, s16 z) {
		piece::Machine part = starter(gamedef, name);
		const std::optional<piece::Machine::Face> by = part.plugFace();
		const v3s16 cell(x == 0 ? 0 : 15, 7, 7);
		m.join({ body, { v3s16(x, 0, z), cell }, v3s16(x == 0 ? -1 : 1, 0, 0) }, std::move(part), *by, piece::Joint::Fixed, 0);
	};
	for (const s16 x : { s16(0), s16(WIDE - 1) }) {
		corner("Steering corner", x, LONG - 1);
		corner("Driven corner", x, 0);
	}
	m.build();
	return m;
}

Json::Value boxOf(const aabb3f &b)
{
	Json::Value out(Json::arrayValue);
	for (const float v : { b.MinEdge.X, b.MinEdge.Y, b.MinEdge.Z, b.MaxEdge.X, b.MaxEdge.Y, b.MaxEdge.Z })
		out.append(v);
	return out;
}

aabb3f readBox(const Json::Value &b)
{
	return aabb3f(b[0].asFloat(), b[1].asFloat(), b[2].asFloat(), b[3].asFloat(), b[4].asFloat(), b[5].asFloat());
}

Json::Value vec(v3f v)
{
	Json::Value out(Json::arrayValue);
	out.append(v.X);
	out.append(v.Y);
	out.append(v.Z);
	return out;
}

v3f readVec(const Json::Value &v) { return v3f(v[0].asFloat(), v[1].asFloat(), v[2].asFloat()); }

Json::Value bake(const piece::Machine &m, const Materials &materials, IGameDef *gamedef)
{
	const physics::Assembly a = compile(m, materials, gamedef);
	Json::Value out;
	for (const physics::Body &b : a.bodies) {
		Json::Value body;
		for (const physics::Box &x : b.boxes) {
			Json::Value box = boxOf(x.box);
			box.append(x.density);
			box.append(x.friction);
			body["boxes"].append(box);
		}
		if (b.round) {
			body["round"]["centre"] = vec(b.round->centre);
			body["round"]["axis"] = vec(b.round->axis);
			body["round"]["radius"] = b.round->radius;
			body["round"]["half_width"] = b.round->half_width;
		}
		out["bodies"].append(body);
	}
	for (const physics::Hinge &h : a.hinges) {
		Json::Value j;
		j["a"] = (Json::UInt64)h.a;
		j["b"] = (Json::UInt64)h.b;
		j["anchor"] = vec(h.anchor);
		j["axis"] = vec(h.axis);
		j["friction"] = h.friction;
		j["limit"] = h.limit;
		out["hinges"].append(j);
	}
	for (const physics::Wheel &w : a.wheels) {
		Json::Value j;
		j["body"] = (Json::UInt64)w.body;
		j["centre"] = vec(w.centre);
		j["axis"] = vec(w.axis);
		j["radius"] = w.radius;
		j["half_width"] = w.half_width;
		j["friction"] = w.friction;
		j["steers"] = w.steers;
		j["driven"] = w.driven;
		j["grip"] = w.grip;
		j["travel"] = w.travel;
		j["sag"] = w.sag;
		out["wheels"].append(j);
	}
	for (const physics::Hub &h : a.hubs) {
		Json::Value j;
		j["body"] = (Json::UInt64)h.body;
		if (h.wheel)
			j["wheel"] = (Json::UInt64)*h.wheel;
		out["hubs"].append(j);
	}
	if (a.seat)
		out["seat"] = vec(*a.seat);
	out["facing"] = vec(a.facing);
	out["power"] = a.power;
	out["rpm"] = a.rpm;

	// what each body looks like: its boxes as compile's, each with its block's tiles
	const NodeDefManager *ndef = gamedef->ndef();
	for (const std::vector<u64> &group : m.rigid()) {
		Json::Value look(Json::arrayValue);
		for (const u64 id : group) {
			const piece::Machine::Part &p = *m.part(id);
			const core::matrix4 at = p.pose.matrix();
			p.object.built().each([&](v3s16 n, MapNode node, const shape::Shape *s) {
				const ContentFeatures &f = ndef->get(node);
				std::vector<aabb3f> boxes;
				if (s)
					s->boxes(boxes);
				else
					boxes.push_back(aabb3f(v3f(-BS / 2), v3f(BS / 2)));
				for (aabb3f b : boxes) {
					b.MinEdge += intToFloat(n, BS);
					b.MaxEdge += intToFloat(n, BS);
					at.transformBoxEx(b);
					b.MinEdge /= BS;
					b.MaxEdge /= BS;
					Json::Value part;
					part["box"] = boxOf(b);
					for (const int t : { 0, 1, 2 })
						part["tiles"].append(f.tiledef[t].name);
					look.append(part);
				}
			});
		}
		if (!look.empty())
			out["looks"].append(look);
	}
	return out;
}

} // namespace

Json::Value bakeShowcase(IGameDef *gamedef, const Materials &materials)
{
	Json::Value out;
	out["car"] = bake(car(gamedef), materials, gamedef);
	return out;
}

physics::Assembly assemblyOf(const Json::Value &machine)
{
	if (!machine["bodies"].isArray())
		throw BaseException("showcase: not a baked machine (bun palette)");
	physics::Assembly a;
	for (const Json::Value &b : machine["bodies"]) {
		physics::Body body;
		for (const Json::Value &x : b["boxes"])
			body.boxes.push_back({ readBox(x), x[6].asFloat(), x[7].asFloat() });
		if (b.isMember("round"))
			body.round = physics::Round{ readVec(b["round"]["centre"]), readVec(b["round"]["axis"]), b["round"]["radius"].asFloat(), b["round"]["half_width"].asFloat() };
		a.bodies.push_back(std::move(body));
	}
	for (const Json::Value &j : machine["hinges"])
		a.hinges.push_back({ j["a"].asUInt64(), j["b"].asUInt64(), readVec(j["anchor"]), readVec(j["axis"]), j["friction"].asFloat(), j["limit"].asFloat() });
	for (const Json::Value &j : machine["wheels"]) {
		physics::Wheel w{ j["body"].asUInt64(), readVec(j["centre"]), readVec(j["axis"]), j["radius"].asFloat(), j["half_width"].asFloat(), j["friction"].asFloat(),
			j["steers"].asBool(), j["driven"].asBool(), j["grip"].asFloat() };
		w.travel = j["travel"].asFloat();
		w.sag = j["sag"].asFloat();
		a.wheels.push_back(w);
	}
	for (const Json::Value &j : machine["hubs"])
		a.hubs.push_back({ j["body"].asUInt64(), j.isMember("wheel") ? std::optional<size_t>(j["wheel"].asUInt64()) : std::nullopt });
	if (machine.isMember("seat"))
		a.seat = readVec(machine["seat"]);
	a.facing = readVec(machine["facing"]);
	a.power = machine["power"].asFloat();
	a.rpm = machine["rpm"].asFloat();
	return a;
}

} // namespace workshop
