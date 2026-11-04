// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "piece/machine.h"

#include "exceptions.h"
#include "piece/frame.h"
#include "piece/header.h"
#include "util/serialize.h"
#include <algorithm>
#include <set>
#include <cmath>
#include <map>
#include <sstream>

namespace piece {

const JointKind JOINTS[size_t(Joint::Count)] = {
	{ "fixed", "fixed", false, 0, false, false },
	{ "rotary", "rotary", true, 0, false, false },
	{ "hinge", "hinge", true, 90, false, false },
	{ "steers", "steers", true, 0, true, false },
	{ "driven", "driven", true, 0, false, true },
	{ "steers_driven", "steers, driven", true, 0, true, true },
	{ "sprung", "sprung", false, 0, false, false, true },
};

// a car's: a hand's breadth to most of a block; a soft road car to a racer
const SpringStep TRAVELS[SPRING_STEPS] = {
	{ "short", "short travel", 0.25f },
	{ "medium", "medium travel", 0.5f },
	{ "long", "long travel", 0.8f },
};
const SpringStep FIRMNESSES[SPRING_STEPS] = {
	{ "soft", "soft", 1.2f },
	{ "medium", "medium", 1.7f },
	{ "firm", "firm", 2.4f },
};

namespace {

const char MAGIC[] = "TFM6";
const char MAGIC_5[] = "TFM5";
const char MAGIC_4[] = "TFM4";
const char MAGIC_3[] = "TFM3";
const char MAGIC_2[] = "TFM2";
const char MAGIC_1[] = "TFM1";
constexpr f32 STEPS = 16; // sixteenths a node

// A part's box (its nodes, local: node n spans n ± 0.5) set in the
// machine's (the same)
std::optional<aabb3f> placed(const std::optional<aabb3f> &local, const Pose &pose)
{
	if (!local)
		return std::nullopt;
	std::optional<aabb3f> out;
	for (int c = 0; c < 8; c++) {
		const v3f corner(c & 1 ? local->MaxEdge.X : local->MinEdge.X, c & 2 ? local->MaxEdge.Y : local->MinEdge.Y,
				c & 4 ? local->MaxEdge.Z : local->MinEdge.Z);
		const v3f at = pose.apply((corner + v3f(0.5f)) * STEPS) / STEPS - v3f(0.5f);
		if (out)
			out->addInternalPoint(at);
		else
			out = aabb3f(at, at);
	}
	return out;
}

std::optional<aabb3f> merged(std::optional<aabb3f> a, const std::optional<aabb3f> &b)
{
	if (!b)
		return a;
	if (!a)
		return b;
	a->addInternalBox(*b);
	return a;
}

struct Texts
{
	std::optional<u64> id;
	struct Part
	{
		Pose pose;
		std::string object;
	};
	std::vector<Part> parts;
	std::vector<Machine::Link> links;
	std::vector<Machine::Socket> sockets;
	v3s16 front{ 0, 0, 1 }; // before fronts (TFM3 and older): its grid's +z
	std::optional<Machine::Plug> plug;
	f32 scale = 1; // before scale (TFM5 and older): full size
};

Texts texts(const std::string &text)
{
	const bool first = text.compare(0, 4, MAGIC_1) == 0, second = text.compare(0, 4, MAGIC_2) == 0,
			third = text.compare(0, 4, MAGIC_3) == 0, fourth = text.compare(0, 4, MAGIC_4) == 0,
			fifth = text.compare(0, 4, MAGIC_5) == 0;
	if (!first && !second && !third && !fourth && !fifth && text.compare(0, 4, MAGIC) != 0) // an object's, or a piece's: one part
		return { std::nullopt, { { Pose{ v3s32(0), 0 }, text } }, {}, {} };
	std::istringstream is(text, std::ios::binary);
	is.ignore(4);
	Texts out;
	out.id = readU64(is);
	if (*out.id == 0)
		throw SerializationError("machine: no id");
	if (!first && !second && !third && !fourth && !fifth) {
		out.scale = readF32(is);
		if (!(out.scale > 0 && out.scale <= 1))
			throw SerializationError("machine: its scale isn't one");
	}
	out.parts.resize(readU16(is));
	for (Texts::Part &p : out.parts) {
		p.pose.offset = readV3S32(is);
		p.pose.turn = readU8(is);
		turn(p.pose.turn); // throws on one that isn't
		if (first) { // what it was made as, and its radius
			readU8(is);
			readU16(is);
		}
		p.object = deSerializeString32(is);
	}
	out.links.resize(readU16(is));
	for (Machine::Link &l : out.links) {
		l.a = readU64(is);
		l.b = readU64(is);
		const u8 joint = readU8(is);
		if (joint >= u8(Joint::Count))
			throw SerializationError("machine: no such joint");
		l.joint = Joint(joint);
		l.anchor = readV3F32(is);
		l.axis = readV3S16(is);
		if (std::abs(l.axis.X) + std::abs(l.axis.Y) + std::abs(l.axis.Z) != 1)
			throw SerializationError("machine: a joint's axis isn't one");
		if (!first && !second && !third && !fourth) { // its spring (TFM5)
			l.spring.travel = readU8(is);
			l.spring.firmness = readU8(is);
			if (l.spring.travel >= SPRING_STEPS || l.spring.firmness >= SPRING_STEPS)
				throw SerializationError("machine: a joint's spring isn't one");
		}
	}
	if (!first && !second) {
		out.sockets.resize(readU16(is));
		for (Machine::Socket &k : out.sockets) {
			k.id = readU64(is);
			k.part = readU64(is);
			k.point = readV3F32(is);
			k.axis = readV3S16(is);
			const u8 joint = readU8(is);
			if (joint >= u8(Joint::Count))
				throw SerializationError("machine: no such joint for a socket");
			k.joint = Joint(joint);
			if (std::abs(k.axis.X) + std::abs(k.axis.Y) + std::abs(k.axis.Z) != 1)
				throw SerializationError("machine: a socket's axis isn't one");
		}
	}
	if (!first && !second && !third) {
		out.front = readV3S16(is);
		if (out.front.Y != 0 || std::abs(out.front.X) + std::abs(out.front.Z) != 1)
			throw SerializationError("machine: its front isn't a level axis");
	}
	// how a copy hung (sprint 76): at its end, if it's one
	if (!first && !second && !third && !fourth && is.peek() != std::char_traits<char>::eof()) {
		Machine::Plug p;
		const u8 joint = readU8(is);
		if (joint >= u8(Joint::Count))
			throw SerializationError("machine: no such joint for its plug");
		p.joint = Joint(joint);
		p.spring.travel = readU8(is);
		p.spring.firmness = readU8(is);
		if (p.spring.travel >= SPRING_STEPS || p.spring.firmness >= SPRING_STEPS)
			throw SerializationError("machine: its plug's spring isn't one");
		// the face it hung by (sprint 77), if it says
		if (is.peek() != std::char_traits<char>::eof()) {
			Machine::Plug::By by;
			by.at = readV3F32(is);
			by.out = readV3S16(is);
			if (std::abs(by.out.X) + std::abs(by.out.Y) + std::abs(by.out.Z) != 1)
				throw SerializationError("machine: its plug's face isn't on an axis");
			p.by = by;
		}
		out.plug = p;
	}
	return out;
}

} // namespace

v3f Pose::apply(v3f local) const
{
	return piece::turn(turn).apply(local) + v3f(offset.X, offset.Y, offset.Z);
}

v3f Pose::unapply(v3f machine) const
{
	// a square turn's inverse is its transpose
	const Turn &t = piece::turn(turn);
	const v3f d = machine - v3f(offset.X, offset.Y, offset.Z);
	v3f out;
	for (int a = 0; a < 3; a++)
		out[a] = d.dotProduct(v3f::from(t.axes[a]));
	return out;
}

core::matrix4 Pose::matrix() const
{
	// a frame's origin is its node (0, 0, 0)'s middle, half a node in from
	// its corner: the part's taken to the machine's
	const v3f half(STEPS / 2);
	core::matrix4 m = piece::turn(turn).matrix();
	m.setTranslation((apply(half) - half) / STEPS * BS);
	return m;
}

Machine::Machine(IGameDef *gamedef) : m_gamedef(gamedef), m_id(freshId()) {}

void Machine::read(const std::string &text)
{
	Texts t = texts(text);
	std::vector<Part> parts;
	for (const Texts::Part &p : t.parts) {
		Object o(m_gamedef);
		o.read(p.object);
		parts.push_back({ std::move(o), p.pose });
	}
	for (const Link &l : t.links)
		if (std::find_if(parts.begin(), parts.end(), [&](const Part &p) { return p.object.id() == l.a; }) == parts.end()
				|| std::find_if(parts.begin(), parts.end(), [&](const Part &p) { return p.object.id() == l.b; }) == parts.end())
			throw SerializationError("machine: a joint to a part it hasn't");
	for (const Socket &k : t.sockets)
		if (std::find_if(parts.begin(), parts.end(), [&](const Part &p) { return p.object.id() == k.part; }) == parts.end())
			throw SerializationError("machine: a socket on a part it hasn't");
	m_parts = std::move(parts);
	m_links = std::move(t.links);
	m_sockets = std::move(t.sockets);
	m_front = t.front;
	m_plug = t.plug;
	m_scale = t.scale;
	// one part from before machines: the machine's id is the object's (as
	// its header says)
	m_id = t.id ? *t.id : m_parts.front().object.id();
}

std::string Machine::write() const
{
	std::ostringstream os(std::ios::binary);
	os.write(MAGIC, 4);
	writeU64(os, m_id);
	writeF32(os, m_scale);
	writeU16(os, u16(m_parts.size()));
	for (const Part &p : m_parts) {
		writeV3S32(os, p.pose.offset);
		writeU8(os, p.pose.turn);
		os << serializeString32(p.object.write());
	}
	writeU16(os, u16(m_links.size()));
	for (const Link &l : m_links) {
		writeU64(os, l.a);
		writeU64(os, l.b);
		writeU8(os, u8(l.joint));
		writeV3F32(os, l.anchor);
		writeV3S16(os, l.axis);
		writeU8(os, l.spring.travel);
		writeU8(os, l.spring.firmness);
	}
	writeU16(os, u16(m_sockets.size()));
	for (const Socket &k : m_sockets) {
		writeU64(os, k.id);
		writeU64(os, k.part);
		writeV3F32(os, k.point);
		writeV3S16(os, k.axis);
		writeU8(os, u8(k.joint));
	}
	writeV3S16(os, m_front);
	if (m_plug) {
		writeU8(os, u8(m_plug->joint));
		writeU8(os, m_plug->spring.travel);
		writeU8(os, m_plug->spring.firmness);
		if (m_plug->by) {
			writeV3F32(os, m_plug->by->at);
			writeV3S16(os, m_plug->by->out);
		}
	}
	return os.str();
}

void Machine::setScale(f32 scale)
{
	if (!(scale > 0 && scale <= 1))
		throw BaseException("Machine::setScale: not a scale");
	m_scale = scale;
}

core::matrix4 Machine::scaling() const
{
	core::matrix4 m;
	m.setScale(v3f(m_scale));
	return m;
}

void Machine::setFront(v3s16 front)
{
	if (front.Y != 0 || std::abs(front.X) + std::abs(front.Z) != 1)
		throw BaseException("Machine::setFront: not a level axis");
	m_front = front;
}

void Machine::renew()
{
	m_id = freshId();
	for (Part &p : m_parts) {
		const u64 was = p.object.id();
		p.object.renew();
		for (Link &l : m_links) {
			if (l.a == was)
				l.a = p.object.id();
			if (l.b == was)
				l.b = p.object.id();
		}
		for (Socket &k : m_sockets)
			if (k.part == was)
				k.part = p.object.id();
	}
	for (Socket &k : m_sockets)
		k.id = freshId();
}

Machine::Part *Machine::part(u64 id)
{
	for (Part &p : m_parts)
		if (p.object.id() == id)
			return &p;
	return nullptr;
}

const Machine::Part *Machine::part(u64 id) const
{
	for (const Part &p : m_parts)
		if (p.object.id() == id)
			return &p;
	return nullptr;
}

Machine::Part &Machine::add(Part part)
{
	m_parts.push_back(std::move(part));
	return m_parts.back();
}

Machine::Part Machine::take(u64 id)
{
	const auto it = std::find_if(m_parts.begin(), m_parts.end(), [&](const Part &p) { return p.object.id() == id; });
	if (it == m_parts.end())
		throw BaseException("Machine::take: no part " + std::to_string(id));
	Part out = std::move(*it);
	m_parts.erase(it);
	m_links.erase(std::remove_if(m_links.begin(), m_links.end(), [&](const Link &l) { return l.a == id || l.b == id; }),
			m_links.end());
	m_sockets.erase(std::remove_if(m_sockets.begin(), m_sockets.end(), [&](const Socket &k) { return k.part == id; }),
			m_sockets.end());
	return out;
}

Machine Machine::extract(u64 id) const
{
	if (!part(id))
		throw BaseException("Machine::extract: no part " + std::to_string(id));
	Machine out(m_gamedef);
	out.read(write());
	out.m_id = freshId();
	// what the first part reaches without crossing it stays behind
	std::set<u64> behind;
	if (m_parts.front().object.id() != id) {
		std::vector<u64> todo{ m_parts.front().object.id() };
		behind.insert(todo.front());
		while (!todo.empty()) {
			const u64 at = todo.back();
			todo.pop_back();
			for (const Link &l : m_links) {
				const u64 other = l.a == at ? l.b : l.b == at ? l.a : 0;
				if (other && other != id && behind.insert(other).second)
					todo.push_back(other);
			}
		}
	}
	// how it hung from what stays behind: it's joined so again
	out.m_plug.reset();
	for (const Link &l : m_links)
		if ((l.a == id && behind.count(l.b)) || (l.b == id && behind.count(l.a))) {
			// the joint's axis is out of a's face: toward b
			const v3s16 toward = l.a == id ? l.axis : v3s16(-l.axis.X, -l.axis.Y, -l.axis.Z);
			out.m_plug = Plug{ l.joint, l.spring, Plug::By{ l.anchor, toward } };
			break;
		}
	for (const Part &p : m_parts)
		if (behind.count(p.object.id()))
			out.take(p.object.id());
	const auto first = std::find_if(out.m_parts.begin(), out.m_parts.end(),
			[&](const Part &p) { return p.object.id() == id; });
	std::rotate(out.m_parts.begin(), first, first + 1);
	// moved, not turned: its corner at the copy's
	const v3s32 by = -out.m_parts.front().pose.offset;
	const v3f byf(by.X, by.Y, by.Z);
	for (Part &p : out.m_parts)
		p.pose.offset += by;
	for (Link &l : out.m_links)
		l.anchor += byf;
	for (Socket &k : out.m_sockets)
		k.point += byf;
	if (out.m_plug && out.m_plug->by)
		out.m_plug->by->at += byf;
	return out;
}

Machine Machine::pull(u64 id)
{
	if (!m_parts.empty() && m_parts.front().object.id() == id)
		throw BaseException("Machine::pull: the first part holds the rest");
	Machine out = extract(id);
	for (const Part &p : out.m_parts)
		take(p.object.id());
	return out;
}

std::optional<aabb3f> Machine::filled(u64 id) const
{
	const Part *p = part(id);
	if (!p)
		throw BaseException("Machine::filled: no part " + std::to_string(id));
	// in its nodes, as boxOf's: node n spans n ± ½
	std::optional<aabb3f> out;
	p->object.design().each([&](v3s16 n, MapNode, const shape::Shape *s) {
		for (s16 x = 0; x < STEPS; x++)
			for (s16 y = 0; y < STEPS; y++)
				for (s16 z = 0; z < STEPS; z++) {
					if (s && !s->has(v3s16(x, y, z)))
						continue;
					const v3f lo = v3f(n.X, n.Y, n.Z) - v3f(0.5f) + v3f(x, y, z) / f32(STEPS);
					const aabb3f cell(lo, lo + v3f(1.f / STEPS));
					if (out)
						out->addInternalBox(cell);
					else
						out = cell;
				}
	});
	return out;
}

std::optional<Machine::Face> Machine::plugFace() const
{
	if (!m_plug || !m_plug->by || m_parts.empty())
		return std::nullopt;
	const Face face = faceAt(m_parts.front().object.id(), m_plug->by->at, m_plug->by->out);
	if ((point(face) - m_plug->by->at).getLengthSQ() > 0.01f)
		throw BaseException("Machine::plugFace: its face isn't on its first part's grid");
	return face;
}

Machine::Face Machine::faceAt(u64 id, v3f at, v3s16 d) const
{
	const Part *p = part(id);
	if (!p)
		throw BaseException("Machine::faceAt: no part " + std::to_string(id));
	// the sixteenth just inside the face, in the part's grid
	const v3f inside = p->pose.unapply(at - v3f::from(d) * 0.5f);
	const v3s32 w(std::floor(inside.X), std::floor(inside.Y), std::floor(inside.Z));
	const v3s32 node(w.X >> 4, w.Y >> 4, w.Z >> 4);
	const Turn &t = turn(p->pose.turn);
	v3s16 local;
	for (int a = 0; a < 3; a++)
		local[a] = s16(d.X * t.axes[a].X + d.Y * t.axes[a].Y + d.Z * t.axes[a].Z);
	return { id, { v3s16(node.X, node.Y, node.Z), v3s16(w.X - node.X * 16, w.Y - node.Y * 16, w.Z - node.Z * 16) },
		local };
}

v3f Machine::point(const Face &face) const
{
	const Part *p = part(face.part);
	if (!p)
		throw BaseException("Machine::point: no part " + std::to_string(face.part));
	const int a = face.normal.X ? 0 : face.normal.Y ? 1 : 2;
	const int sign = face.normal.X + face.normal.Y + face.normal.Z;
	// the part's own sixteenths: the cell's middle, on the face
	const v3s32 cell = v3s32(face.at.node.X, face.at.node.Y, face.at.node.Z) * 16
			+ v3s32(face.at.cell.X, face.at.cell.Y, face.at.cell.Z);
	v3f local;
	for (int b = 0; b < 3; b++)
		local[b] = cell[b] + 0.5f;
	local[a] = cell[a] + (sign > 0 ? 1.f : 0.f);
	return p->pose.apply(local);
}

v3s16 Machine::normal(const Face &face) const
{
	const Part *p = part(face.part);
	if (!p)
		throw BaseException("Machine::normal: no part " + std::to_string(face.part));
	const v3s32 n = turn(p->pose.turn).apply(v3s32(face.normal.X, face.normal.Y, face.normal.Z));
	return v3s16(n.X, n.Y, n.Z);
}

v3s16 Machine::makeRoom(u64 id, v3s32 lo, v3s32 hi)
{
	Part *p = part(id);
	if (!p)
		throw BaseException("Machine::makeRoom: no part " + std::to_string(id));
	// what's there and what's wanted, together
	std::optional<aabb3f> box = boxOf(p->object.design().write());
	v3s32 least = lo, most = hi;
	if (box)
		for (int a = 0; a < 3; a++) {
			least[a] = std::min(least[a], s32(std::lround(box->MinEdge[a] + 0.5f)));
			most[a] = std::max(most[a], s32(std::lround(box->MaxEdge[a] - 0.5f)));
		}
	v3s16 by(0, 0, 0);
	for (int a = 0; a < 3; a++) {
		if (most[a] - least[a] >= SIDE)
			throw BaseException("Machine::makeRoom: wider than a part");
		if (least[a] < 0)
			by[a] = s16(-least[a]);
		else if (most[a] >= SIDE)
			by[a] = s16(SIDE - 1 - most[a]);
	}
	if (by == v3s16(0, 0, 0))
		return by;
	p->object.shift(by);
	p->pose.offset -= turn(p->pose.turn).apply(v3s32(by.X, by.Y, by.Z) * 16);
	return by;
}

Machine::Part &Machine::grow(const Face &face, const std::vector<v3s32> &cells, content_t material, Joint joint)
{
	const Part *from = part(face.part);
	if (!from)
		throw BaseException("Machine::grow: no part " + std::to_string(face.part));
	if (cells.empty())
		throw BaseException("Machine::grow: nothing to grow");
	// its grid's corner the least node the cells are in
	v3s32 least(cells[0].X >> 4, cells[0].Y >> 4, cells[0].Z >> 4);
	for (const v3s32 &c : cells)
		for (int a = 0; a < 3; a++)
			least[a] = std::min(least[a], c[a] >> 4);
	Object o(m_gamedef);
	std::map<v3s16, shape::Shape> nodes;
	for (const v3s32 &c : cells) {
		const v3s32 l = c - least * 16;
		const v3s16 node(l.X >> 4, l.Y >> 4, l.Z >> 4);
		if (node.X >= SIDE || node.Y >= SIDE || node.Z >= SIDE)
			throw BaseException("Machine::grow: wider than a part");
		nodes[node].set(v3s16(l.X & 15, l.Y & 15, l.Z & 15), true);
	}
	for (const auto &[n, s] : nodes) {
		o.design().map().setNode(n, MapNode(material));
		o.design().map().setShape(n, s);
	}
	o.build();
	const v3s32 offset = from->pose.offset + turn(from->pose.turn).apply(least * 16);
	const u64 id = o.id();
	v3f at = point(face);
	const v3s16 n = normal(face);
	if (JOINTS[size_t(joint)].turns) {
		// its axle through the middle of what's drawn, not where the drag
		// began (a wheel turns about its centre)
		v3f lo(cells[0].X, cells[0].Y, cells[0].Z), hi = lo;
		for (const v3s32 &c : cells) {
			lo = v3f(std::min(lo.X, (f32)c.X), std::min(lo.Y, (f32)c.Y), std::min(lo.Z, (f32)c.Z));
			hi = v3f(std::max(hi.X, (f32)c.X), std::max(hi.Y, (f32)c.Y), std::max(hi.Z, (f32)c.Z));
		}
		const v3f middle = from->pose.apply((lo + hi + v3f(1)) * 0.5f);
		for (int a = 0; a < 3; a++)
			if (n[a] == 0)
				at[a] = middle[a];
	}
	m_parts.push_back({ std::move(o), Pose{ offset, from->pose.turn } });
	m_links.push_back({ face.part, id, joint, at, n });
	return m_parts.back();
}

std::vector<std::vector<u64>> Machine::rigid() const
{
	// each part's group, by index: its first part's, joined by welds; what
	// no joint reaches is welded to the first part (it moved with it
	// before there were joints)
	const size_t n = m_parts.size();
	std::vector<size_t> root(n);
	for (size_t i = 0; i < n; i++)
		root[i] = i;
	auto find = [&](size_t i) {
		while (root[i] != i)
			i = root[i] = root[root[i]];
		return i;
	};
	auto weld = [&](size_t a, size_t b) {
		a = find(a), b = find(b);
		root[std::max(a, b)] = std::min(a, b); // the earlier part leads: the same order every time
	};
	auto index = [&](u64 id) {
		for (size_t i = 0; i < n; i++)
			if (m_parts[i].object.id() == id)
				return i;
		throw BaseException("Machine::rigid: a joint to no part " + std::to_string(id));
	};
	std::vector<bool> turning(n, false);
	for (const Link &l : m_links) {
		const size_t a = index(l.a), b = index(l.b);
		// a sprung joint's parts apart too (sprint 76): the hub rides its
		// wheel's spring
		if (JOINTS[size_t(l.joint)].turns || JOINTS[size_t(l.joint)].sprung)
			turning[a] = turning[b] = true;
		else
			weld(a, b);
	}
	for (size_t i = 1; i < n; i++)
		if (!turning[i] && find(i) != 0) {
			bool welded = false;
			for (const Link &l : m_links)
				welded |= index(l.a) == i || index(l.b) == i;
			if (!welded)
				weld(0, i);
		}
	std::vector<std::vector<u64>> groups;
	std::vector<size_t> group(n, n);
	for (size_t i = 0; i < n; i++) {
		const size_t r = find(i);
		if (group[r] == n) {
			group[r] = groups.size();
			groups.emplace_back();
		}
		groups[group[r]].push_back(m_parts[i].object.id());
	}
	return groups;
}

void Machine::relink(u64 a, u64 b, Joint joint, const std::optional<Face> &at)
{
	if (!part(a) || !part(b) || a == b)
		throw BaseException("Machine::relink: not two of its parts");
	for (Link &l : m_links)
		if ((l.a == a && l.b == b) || (l.a == b && l.b == a)) {
			l.joint = joint;
			if (at) {
				l.anchor = point(*at);
				l.axis = normal(*at);
			}
			return;
		}
	if (at)
		m_links.push_back({ a, b, joint, point(*at), normal(*at) });
	else
		m_links.push_back({ a, b, joint, v3f(part(a)->pose.offset.X, part(a)->pose.offset.Y, part(a)->pose.offset.Z),
			v3s16(0, 1, 0) });
}

void Machine::tune(u64 a, u64 b, Spring spring)
{
	if (spring.travel >= SPRING_STEPS || spring.firmness >= SPRING_STEPS)
		throw BaseException("Machine::tune: not a spring");
	for (Link &l : m_links)
		if ((l.a == a && l.b == b) || (l.a == b && l.b == a)) {
			l.spring = spring;
			return;
		}
	throw BaseException("Machine::tune: no joint between them");
}

void Machine::unlink(u64 a, u64 b)
{
	m_links.erase(std::remove_if(m_links.begin(), m_links.end(), [&](const Link &l) {
		return (l.a == a && l.b == b) || (l.a == b && l.b == a);
	}), m_links.end());
}

std::optional<Machine::Face> Machine::faceToward(v3s16 d) const
{
	const std::optional<aabb3f> box = designed();
	if (!box)
		return std::nullopt;
	const v3f dir = v3f::from(d);
	// its middle (sixteenths), from what's filled, not its nodes' box: a
	// wheel 3½ nodes across is met at its middle (sprint 65)
	v3f lo(1e9f), hi(-1e9f);
	for (const Part &p : m_parts)
		p.object.design().each([&](v3s16 n, MapNode, const shape::Shape *s) {
			for (s16 x = 0; x < STEPS; x++)
				for (s16 y = 0; y < STEPS; y++)
					for (s16 z = 0; z < STEPS; z++) {
						if (s && !s->has(v3s16(x, y, z)))
							continue;
						const v3f c = p.pose.apply(v3f(n.X * STEPS + x + 0.5f, n.Y * STEPS + y + 0.5f, n.Z * STEPS + z + 0.5f));
						lo = v3f(std::min(lo.X, c.X), std::min(lo.Y, c.Y), std::min(lo.Z, c.Z));
						hi = v3f(std::max(hi.X, c.X), std::max(hi.Y, c.Y), std::max(hi.Z, c.Z));
					}
		});
	const v3f middle = (lo + hi) / 2;
	// the filled sixteenth that reaches furthest that way (of those, the
	// nearest its middle across): a beam a few sixteenths thick is met at
	// its own face, not its node's (sprint 77: it floated off the other)
	std::optional<Face> best;
	f32 best_far = 0, best_off = 0;
	for (const Part &p : m_parts) {
		// the part's own way along d
		const Turn &t = turn(p.pose.turn);
		v3s16 local;
		for (int a = 0; a < 3; a++)
			local[a] = s16(d.X * t.axes[a].X + d.Y * t.axes[a].Y + d.Z * t.axes[a].Z);
		p.object.design().each([&](v3s16 n, MapNode, const shape::Shape *s) {
			for (s16 x = 0; x < STEPS; x++)
				for (s16 y = 0; y < STEPS; y++)
					for (s16 z = 0; z < STEPS; z++) {
						if (s && !s->has(v3s16(x, y, z)))
							continue;
						const v3f c = p.pose.apply(v3f(n.X * STEPS + x + 0.5f, n.Y * STEPS + y + 0.5f, n.Z * STEPS + z + 0.5f));
						const f32 far = c.dotProduct(dir);
						const v3f across = (c - middle) - dir * (c - middle).dotProduct(dir);
						const f32 off = across.getLengthSQ();
						if (best && (far < best_far - 0.5f || (far < best_far + 0.5f && off >= best_off)))
							continue;
						best = Face{ p.object.id(), { n, v3s16(x, y, z) }, local };
						best_far = far;
						best_off = off;
					}
		});
	}
	return best;
}

Machine::Face Machine::mirrored(const Face &face, int axis, s32 twice) const
{
	const Part *p = part(face.part);
	if (!p)
		throw BaseException("Machine::mirrored: no part " + std::to_string(face.part));
	const Pose &pose = p->pose;
	const v3s32 cell = v3s32(face.at.node.X, face.at.node.Y, face.at.node.Z) * 16
			+ v3s32(face.at.cell.X, face.at.cell.Y, face.at.cell.Z);
	v3f middle = pose.apply(v3f(cell.X, cell.Y, cell.Z) + v3f(0.5f));
	middle[axis] = twice - middle[axis];
	const v3f back = pose.unapply(middle);
	const v3s32 w(std::floor(back.X), std::floor(back.Y), std::floor(back.Z));
	const v3s32 node(w.X >> 4, w.Y >> 4, w.Z >> 4); // floor: whole nodes, and what's left
	// the normal mirrored: turned into the machine's, flipped, turned back
	v3s32 n = turn(pose.turn).apply(v3s32(face.normal.X, face.normal.Y, face.normal.Z));
	n[axis] = -n[axis];
	const Turn &t = turn(pose.turn);
	v3s16 local;
	for (int a = 0; a < 3; a++)
		local[a] = s16(n.X * t.axes[a].X + n.Y * t.axes[a].Y + n.Z * t.axes[a].Z);
	return { face.part, { v3s16(node.X, node.Y, node.Z), v3s16(w.X - node.X * 16, w.Y - node.Y * 16, w.Z - node.Z * 16) },
		local };
}

void Machine::join(const Face &mine, Machine other, const Face &theirs, Joint joint, u8 spin)
{
	const v3f p = point(mine);
	const v3s16 n = normal(mine);
	// a copy welded on is joined as it hung (sprint 76)
	Spring spring;
	if (joint == Joint::Fixed && other.m_plug) {
		joint = other.m_plug->joint;
		spring = other.m_plug->spring;
	}
	adopt(std::move(other), theirs, p, n, spin);
	m_links.push_back({ mine.part, theirs.part, joint, p, n, spring });
}

void Machine::adopt(Machine other, const Face &theirs, v3f p, v3s16 n, u8 spin)
{
	// their sixteenths are made ours, which only means anything at one size
	if (other.m_scale != m_scale)
		throw BaseException("Machine::adopt: parts of different scales");
	const v3f q = other.point(theirs);
	const v3s16 m = other.normal(theirs);
	// their face's normal turned against ours, then spun about it; their
	// point on ours
	u8 t = turnTaking(m, v3s16(-n.X, -n.Y, -n.Z), 0);
	const u8 quarter = quarterAbout(n);
	for (u8 i = 0; i < spin % 4; i++)
		t = compose(t, quarter);
	const Turn &turned = turn(t);
	const v3f shift = p - turned.apply(q);
	const v3s32 by(std::lround(shift.X), std::lround(shift.Y), std::lround(shift.Z));
	auto axis = [&](v3s16 a) {
		const v3s32 v = turned.apply(v3s32(a.X, a.Y, a.Z));
		return v3s16(v.X, v.Y, v.Z);
	};
	for (Part &part : other.m_parts) {
		part.pose.offset = turned.apply(part.pose.offset) + by;
		part.pose.turn = compose(part.pose.turn, t);
		m_parts.push_back(std::move(part));
	}
	for (Link &l : other.m_links) {
		l.anchor = turned.apply(l.anchor) + v3f(by.X, by.Y, by.Z);
		l.axis = axis(l.axis);
		m_links.push_back(l);
	}
	for (Socket &k : other.m_sockets) {
		k.point = turned.apply(k.point) + v3f(by.X, by.Y, by.Z);
		k.axis = axis(k.axis);
		m_sockets.push_back(k);
	}
}

const Machine::Socket &Machine::socket(const Face &face, Joint joint)
{
	m_sockets.push_back({ freshId(), face.part, point(face), normal(face), joint });
	return m_sockets.back();
}

void Machine::unsocket(u64 id)
{
	m_sockets.erase(std::remove_if(m_sockets.begin(), m_sockets.end(), [&](const Socket &k) { return k.id == id; }),
			m_sockets.end());
}

void Machine::plug(u64 id, Machine other, const Face &theirs, u8 spin)
{
	const auto it = std::find_if(m_sockets.begin(), m_sockets.end(), [&](const Socket &k) { return k.id == id; });
	if (it == m_sockets.end())
		throw BaseException("Machine::plug: no socket " + std::to_string(id));
	const Socket k = *it;
	m_sockets.erase(it);
	adopt(std::move(other), theirs, k.point, k.axis, spin);
	m_links.push_back({ k.part, theirs.part, k.joint, k.point, k.axis });
}

void Machine::build()
{
	for (Part &p : m_parts)
		p.object.build();
}

bool Machine::empty() const
{
	for (const Part &p : m_parts)
		if (!p.object.empty())
			return false;
	return true;
}

std::optional<aabb3f> Machine::designed() const
{
	std::optional<aabb3f> out;
	for (const Part &p : m_parts)
		out = merged(out, placed(boxOf(p.object.design().write()), p.pose));
	return out;
}

std::optional<aabb3f> Machine::built() const
{
	std::optional<aabb3f> out;
	for (const Part &p : m_parts)
		out = merged(out, placed(boxOf(p.object.built().write()), p.pose));
	return out;
}

Header header(const std::string &text)
{
	const Texts t = texts(text);
	Header out{ t.id, std::nullopt, std::nullopt };
	for (const Texts::Part &p : t.parts) {
		const ObjectText o = split(p.object);
		out.built = merged(out.built, placed(boxOf(o.built), p.pose));
		out.designed = merged(out.designed, placed(boxOf(o.design), p.pose));
		if (!t.id) // one part, from before machines: its id is the object's
			out.id = o.id;
	}
	for (std::optional<aabb3f> *box : { &out.built, &out.designed })
		if (*box) {
			(*box)->MinEdge *= t.scale;
			(*box)->MaxEdge *= t.scale;
		}
	return out;
}

Fit nearestFit(v3s16 n, std::optional<v3s16> side, const core::matrix4 &held)
{
	v3f wants[3];
	for (int a = 0; a < 3; a++) {
		v3f e(0, 0, 0);
		e[a] = 1;
		held.transformVect(e);
		wants[a] = e - held.getTranslation();
	}
	const v3s16 against(-n.X, -n.Y, -n.Z);
	std::optional<u8> best;
	f32 best_score = 0;
	v3s16 best_side;
	for (u8 t = 0; t < TURN_COUNT; t++) {
		const Turn &T = turn(t);
		// its side that'd meet theirs, turned so
		v3s16 m(0, 0, 0);
		for (int a = 0; a < 3; a++)
			if (T.axes[a] == against || T.axes[a] == n)
				m[a] = T.axes[a] == against ? 1 : -1;
		if (side && m != *side)
			continue;
		f32 score = 0;
		for (int a = 0; a < 3; a++)
			score += v3f::from(T.axes[a]).dotProduct(wants[a]);
		if (!best || score > best_score) {
			best = t;
			best_score = score;
			best_side = m;
		}
	}
	if (!best)
		throw BaseException("nearestFit: no turn sets that side on");
	// the quarters about the joint that make it so, as join turns it
	const u8 quarter = quarterAbout(n);
	u8 spun = turnTaking(best_side, against, 0);
	for (u8 k = 0; k < 4; k++, spun = compose(spun, quarter))
		if (spun == *best)
			return { best_side, k };
	throw BaseException("nearestFit: its turn isn't one about the joint");
}

} // namespace piece
