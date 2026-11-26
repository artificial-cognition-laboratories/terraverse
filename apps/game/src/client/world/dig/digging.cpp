// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/world/dig/digging.h"

#include "src/client/app/shell/host/ui.h"
#include "src/client/world/objects/localplayer.h"
#include "src/client/world/session/client.h"
#include "src/shared/game/content/nodedef.h"
#include "src/shared/game/dig/earthwork.h"
#include "src/shared/game/dig/scoop.h"
#include "src/shared/game/items/itemdef.h"
#include "src/shared/game/pointing/pointedthing.h"
#include "src/shared/map/map.h"
#include <algorithm>
#include <array>
#include <map>
#include <unordered_map>
#include <tuple>
#include <climits>
#include <cmath>

namespace dig {

namespace {
// Where on the node pointed at they aim: its sixteenth, kept in the node.
v3s16 cellOf(const PointedThing &pointed);
Json::Value list(v3s32 v);
} // namespace

bool Digging::menu() const
{
	return holds();
}

void Digging::look(earthwork::Mode mode, Button button)
{
	m_mode = mode;
	m_button = button;
	if (button == Button::Up)
		m_kept.reset();
}

void Digging::look(earthwork::Mode mode, Button button, const core::line3d<f32> &ray)
{
	m_dir = ray.getVector().normalize();
	m_eye = ray.start / BS * shape::STEPS + v3f(8, 8, 8);
	look(mode, button);
}

bool Digging::take(const PointedThing &pointed)
{
	if (bores()) // a bore's paced by swing(), a cell at a time: the engine's dig is only swallowed
		return holds() && pointed.type == POINTEDTHING_NODE && (m_button == Button::Down || holds(pointed));
	const std::optional<earthwork::Target> t = target(pointed);
	if (!t)
		return false;
	Json::Value payload;
	payload["min"] = list(t->min);
	payload["max"] = list(t->max);
	Json::StreamWriterBuilder writer;
	writer["indentation"] = "";
	m_opts.client->sendUiEvent("dig.take", Json::writeString(writer, payload));
	return true;
}

std::vector<aabb3f> Digging::preview(const PointedThing &pointed)
{
	m_shapes.clear(); // read again each frame: the ground may have changed
	const std::optional<earthwork::Target> t = target(pointed);
	if (!t || pointed.type != POINTEDTHING_NODE)
		return {};
	// world sixteenths to BS about the pointed node's centre
	const v3s32 origin = v3s32(pointed.node_undersurface.X, pointed.node_undersurface.Y, pointed.node_undersurface.Z) * shape::STEPS;
	const auto at = [&](s32 c, s32 o) { return (f32(c - o) / shape::STEPS - 0.5f) * BS; };
	// a level is its plane, where it'll be cut down to; else the box
	const s32 top = t->kind == earthwork::Kind::Level ? t->min.Y : t->max.Y + 1;
	return { aabb3f(at(t->min.X, origin.X), at(t->min.Y, origin.Y), at(t->min.Z, origin.Z),
			at(t->max.X + 1, origin.X), at(top, origin.Y), at(t->max.Z + 1, origin.Z)) };
}

std::optional<earthwork::Target> Digging::target(const PointedThing &pointed)
{
	if (!holds())
		return std::nullopt;
	if (m_kept && m_button == Button::Down)
		return m_kept;
	const auto [size, shape] = chosen();
	if (!holds(pointed) || !size.isObject())
		return std::nullopt;
	LocalPlayer *player = m_opts.client->getEnv().getLocalPlayer();
	const v3s32 aim = aimOf(pointed);
	const s32 feet = (s32)std::floor((player->getPosition().Y / BS + 0.5f) * shape::STEPS);
	const earthwork::Kind kind = shape == "cell" ? earthwork::Kind::Cell : shape == "round" ? earthwork::Kind::Round :
			shape == "level" ? earthwork::Kind::Level : earthwork::Kind::Box;
	const earthwork::Target t = earthwork::place(aim, surface(aim.X, aim.Z, aim.Y), feet, player->getYaw() * core::DEGTORAD,
			{ size["along"].asInt(), size["across"].asInt(), size["deep"].asInt() }, kind, m_mode);
	// a form's kept while held, so it's dug out as outlined; a cell isn't: a sweep digs what it passes
	if (m_button == Button::Down && kind != earthwork::Kind::Cell)
		m_kept = t;
	return t;
}

bool Digging::bores() const
{
	return chosen().second == "bore";
}

void Digging::swing(f32 dtime)
{
	// what's been asked for is forgotten after a while: dug by then, or refused
	for (auto it = m_sent.begin(); it != m_sent.end();)
		it = (it->second -= dtime) <= 0 ? m_sent.erase(it) : std::next(it);
	const std::optional<Tool> t = tool();
	if (m_button == Button::Up || !t || m_next.empty()) {
		m_owed = 0;
		return;
	}
	// a cell at a time, evenly: the tool's rate a second, never two together
	m_owed = std::min(m_owed + dtime * t->rate, 1.0f);
	if (m_owed < 1)
		return;
	m_owed -= 1;
	const v3s32 c = m_next.front();
	m_sent[c] = SENT;
	Json::Value payload;
	payload["cell"] = list(c);
	Json::StreamWriterBuilder writer;
	writer["indentation"] = "";
	m_opts.client->sendUiEvent("dig.take", Json::writeString(writer, payload));
}

std::optional<Digging::Bore> Digging::bore(const PointedThing &pointed)
{
	m_next.clear();
	if (!bores() || !holds() || !tool() || pointed.type != POINTEDTHING_NODE || (m_button == Button::Up && !holds(pointed)))
		return std::nullopt;
	m_shapes.clear(); // read again each frame: the ground may have changed
	const Json::Value size = chosen().first;
	const f32 r = size["across"].asInt() / 2.0f, reach = size["along"].asInt();
	Bore out;
	out.node = pointed.node_undersurface;
	// world sixteenths to BS about the pointed node's centre
	const v3f origin = v3f(out.node.X, out.node.Y, out.node.Z) * shape::STEPS + v3f(8, 8, 8);
	const auto local = [&](v3f s) { return (s - origin) / shape::STEPS * BS; };
	const v3f hit = pointed.intersection_point / BS * shape::STEPS + v3f(8, 8, 8);
	// the ring: a circle the bore's width where they aim, cast from their eye as a cone onto every
	// face of the sixteenths they can see in it (floor, wall, corner), outlined where those end: an
	// edge only one of them has. What's hidden behind is hidden by the depth buffer as it's drawn.
	const f32 far = (hit - m_eye).getLength();
	const auto axis = [](auto &v, int i) -> auto & { return i == 0 ? v.X : i == 1 ? v.Y : v.Z; };
	// where the cone can be, SLOPE before and after the aim: the box round its two ends' discs
	const f32 t0 = std::max(1.0f, far - SLOPE), t1 = far + SLOPE, wide = r * t1 / far + 2;
	const v3f e0 = m_eye + m_dir * t0, e1 = m_eye + m_dir * t1;
	v3s32 lo, hi;
	for (int i = 0; i < 3; i++) {
		axis(lo, i) = (s32)std::floor(std::min(axis(e0, i), axis(e1, i)) - wide);
		axis(hi, i) = (s32)std::ceil(std::max(axis(e0, i), axis(e1, i)) + wide);
	}
	// what's filled in it, read a block at a time (a block's shape once, not a sixteenth's)
	const Grid grid = fill(lo, hi);
	std::unordered_map<std::array<s32, 6>, int, EdgeHash> edges;
	for (s32 y = lo.Y + 1; y < hi.Y; y++)
		for (s32 z = lo.Z + 1; z < hi.Z; z++)
			for (s32 x = lo.X + 1; x < hi.X; x++) {
				const v3f mid = v3f(x, y, z) + v3f(0.5f, 0.5f, 0.5f) - m_eye;
				const f32 t = mid.dotProduct(m_dir);
				const f32 cone = r * t / far + 1; // a sixteenth's slack: its faces' middles
				if (t <= 0 || (mid - m_dir * t).getLengthSQ() > cone * cone || !grid.at(v3s32(x, y, z)))
					continue;
				for (int ax = 0; ax < 3; ax++)
					for (s32 side : { -1, 1 }) {
						v3s32 n(x, y, z);
						axis(n, ax) += side;
						v3f centre = v3f(x, y, z) + v3f(0.5f, 0.5f, 0.5f);
						axis(centre, ax) += 0.5f * side;
						const v3f toward = centre - m_eye;
						const f32 ft = toward.dotProduct(m_dir);
						if (axis(toward, ax) * side >= 0 || ft <= 0 || (toward - m_dir * ft).getLengthSQ() > (r * ft / far) * (r * ft / far) ||
								grid.at(n))
							continue; // facing away, outside the circle, or covered
						// its four edges, by their ends (integer sixteenths)
						const int b1 = (ax + 1) % 3, b2 = (ax + 2) % 3;
						v3s32 base(x, y, z);
						axis(base, ax) += side > 0 ? 1 : 0;
						const auto corner = [&](int i, int j) {
							v3s32 c = base;
							axis(c, b1) += i;
							axis(c, b2) += j;
							return c;
						};
						const std::pair<std::pair<int, int>, std::pair<int, int>> sides[] = {
							{ { 0, 0 }, { 1, 0 } }, { { 0, 1 }, { 1, 1 } }, { { 0, 0 }, { 0, 1 } }, { { 1, 0 }, { 1, 1 } } };
						for (const auto &[e1, e2] : sides) {
							v3s32 p = corner(e1.first, e1.second), q = corner(e2.first, e2.second);
							if (std::tie(q.X, q.Y, q.Z) < std::tie(p.X, p.Y, p.Z))
								std::swap(p, q);
							edges[{ p.X, p.Y, p.Z, q.X, q.Y, q.Z }]++;
						}
					}
			}
	for (const auto &[e, count] : edges) {
		if (count != 1)
			continue;
		// a touch toward them, so the ground doesn't hide it
		const v3f p(e[0], e[1], e[2]), q(e[3], e[4], e[5]);
		const auto lift = [&](v3f v) { return local(v + (m_eye - v).normalize() * 0.15f); };
		out.ring.emplace_back(lift(p), lift(q));
	}
	// the next cells it'll take, in order, as the server will (server/game/dig)
	const s32 cell = tool()->cell;
	m_next.clear();
	for (const v3s32 &c : earthwork::bore(m_eye, m_dir, r, std::min(reach, (hit - m_eye).getLength() + r + 2 * cell), cell)) {
		if ((int)out.next.size() == NEXT)
			break;
		if (m_sent.count(c))
			continue; // asked for: as good as gone
		bool there = false;
		for (s32 y = c.Y; !there && y < c.Y + cell; y++)
			for (s32 z = c.Z; !there && z < c.Z + cell; z++)
				for (s32 x = c.X; !there && x < c.X + cell; x++)
					there = solid(v3s32(x, y, z));
		if (!there)
			continue;
		const MapNode n = m_opts.client->getEnv().getMap().getNode(v3s16(c.X >> 4, c.Y >> 4, c.Z >> 4));
		if (!m_opts.client->ndef()->get(n).groups.count("loose"))
			break; // it doesn't dig past what it can't move
		const v3f lo(c.X, c.Y, c.Z);
		out.next.push_back(aabb3f(local(lo), local(lo + v3f(cell, cell, cell))));
		m_next.push_back(c);
	}
	return out;
}

bool Digging::holds() const
{
	LocalPlayer *player = m_opts.client->getEnv().getLocalPlayer();
	if (!player)
		return false;
	ItemStack selected, hand;
	const ItemStack &held = player->getWieldedItem(&selected, &hand);
	return itemgroup_get(m_opts.client->idef()->get(held.name).groups, "shovel") > 0;
}

bool Digging::holds(const PointedThing &pointed) const
{
	if (!holds() || pointed.type != POINTEDTHING_NODE)
		return false;
	const MapNode n = m_opts.client->getEnv().getMap().getNode(pointed.node_undersurface);
	return m_opts.client->ndef()->get(n).groups.count("loose") > 0;
}

std::pair<Json::Value, std::string> Digging::chosen() const
{
	const Json::Value &dig = m_opts.ui.get("dig");
	const Json::Value &c = dig["chosen"];
	for (const Json::Value &o : dig["operations"])
		for (const Json::Value &z : o["sizes"])
			if (c.isObject() && o["id"] == c["operation"] && z["id"] == c["size"])
				return { z, o["shape"].asString() };
	return { dig["operations"][0]["sizes"][0], dig["operations"][0]["shape"].asString() };
}

s32 Digging::surface(s32 x, s32 z, s32 near) const
{
	return earthwork::surface(x, z, near, [&](v3s32 c) { return solid(c); }).value_or(near);
}

Digging::Grid Digging::fill(v3s32 lo, v3s32 hi) const
{
	Grid g{ lo, hi - lo + v3s32(1, 1, 1), {} };
	g.filled.assign((size_t)g.size.X * g.size.Y * g.size.Z, false);
	Map &map = m_opts.client->getEnv().getMap();
	const NodeDefManager *ndef = m_opts.client->ndef();
	for (s16 ny = (s16)(lo.Y >> 4); ny <= (s16)(hi.Y >> 4); ny++)
		for (s16 nz = (s16)(lo.Z >> 4); nz <= (s16)(hi.Z >> 4); nz++)
			for (s16 nx = (s16)(lo.X >> 4); nx <= (s16)(hi.X >> 4); nx++) {
				const v3s16 p(nx, ny, nz);
				bool valid;
				const MapNode n = map.getNode(p, &valid);
				if (!valid || ndef->get(n).buildable_to)
					continue;
				const std::optional<shape::Shape> s = map.solidShape(p);
				const v3s32 base = v3s32(nx, ny, nz) * shape::STEPS;
				for (s32 y = std::max(lo.Y, base.Y); y <= std::min(hi.Y, base.Y + 15); y++)
					for (s32 z = std::max(lo.Z, base.Z); z <= std::min(hi.Z, base.Z + 15); z++)
						for (s32 x = std::max(lo.X, base.X); x <= std::min(hi.X, base.X + 15); x++)
							if (!s || s->has(v3s16(x - base.X, y - base.Y, z - base.Z)))
								g.filled[g.index(v3s32(x, y, z))] = true;
			}
	return g;
}

bool Digging::solid(v3s32 c) const
{
	Map &map = m_opts.client->getEnv().getMap();
	const v3s16 p(c.X >> 4, c.Y >> 4, c.Z >> 4);
	auto known = m_shapes.find(p);
	if (known == m_shapes.end()) {
		bool valid;
		const MapNode n = map.getNode(p, &valid);
		known = m_shapes.emplace(p, !valid || m_opts.client->ndef()->get(n).buildable_to ? std::nullopt :
				std::optional<std::optional<shape::Shape>>(map.solidShape(p))).first;
	}
	if (!known->second)
		return false;
	const std::optional<shape::Shape> &s = *known->second;
	return !s || s->has(v3s16(c.X & 15, c.Y & 15, c.Z & 15));
}

std::optional<Digging::Tool> Digging::tool() const
{
	LocalPlayer *player = m_opts.client->getEnv().getLocalPlayer();
	ItemStack selected, hand;
	const Json::Value &t = m_opts.ui.get("dig")["tools"][player->getWieldedItem(&selected, &hand).name];
	if (!t["cell"].isInt() || !t["rate"].isNumeric())
		return std::nullopt; // not told yet
	return Tool{ t["cell"].asInt(), t["rate"].asFloat() };
}

v3s32 Digging::aimOf(const PointedThing &pointed)
{
	const v3s16 n = pointed.node_undersurface, c = cellOf(pointed);
	return v3s32(n.X, n.Y, n.Z) * shape::STEPS + v3s32(c.X, c.Y, c.Z);
}

namespace {
v3s16 cellOf(const PointedThing &pointed)
{
	const v3s16 node = pointed.node_undersurface, face = pointed.node_abovesurface - node;
	const v3s16 c = shape::hit(node, pointed.intersection_point, face).cell;
	return v3s16(std::clamp<s16>(c.X, 0, shape::STEPS - 1), std::clamp<s16>(c.Y, 0, shape::STEPS - 1),
			std::clamp<s16>(c.Z, 0, shape::STEPS - 1));
}

Json::Value list(v3s32 v)
{
	Json::Value out(Json::arrayValue);
	out.append(v.X);
	out.append(v.Y);
	out.append(v.Z);
	return out;
}
} // namespace

} // namespace dig
