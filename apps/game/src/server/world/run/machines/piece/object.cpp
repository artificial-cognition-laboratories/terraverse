// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/server/world/run/machines/piece/object.h"

#include "src/shared/core/exceptions.h"
#include "src/shared/util/serialize.h"
#include <random>
#include <sstream>

namespace piece {

namespace {

const char MAGIC[] = "TFO1";

} // namespace

u64 freshId()
{
	static std::mt19937_64 random{ std::random_device{}() };
	u64 id;
	do
		id = random();
	while (id == 0);
	return id;
}

ObjectText split(const std::string &text)
{
	if (text.compare(0, 4, MAGIC) != 0) // a piece from before objects
		return { std::nullopt, text, text };
	std::istringstream is(text, std::ios::binary);
	is.ignore(4);
	ObjectText out;
	out.id = readU64(is);
	if (*out.id == 0)
		throw SerializationError("object: no id");
	out.design = deSerializeString32(is);
	out.built = deSerializeString32(is);
	return out;
}

Object::Object(IGameDef *gamedef) :
	m_gamedef(gamedef), m_id(freshId()), m_design(gamedef), m_built(gamedef)
{
}

void Object::read(const std::string &text)
{
	const ObjectText p = split(text);
	m_design.read(p.design);
	m_built.read(p.built);
	m_id = p.id ? *p.id : freshId();
}

std::string Object::write() const
{
	std::ostringstream os(std::ios::binary);
	os.write(MAGIC, 4);
	writeU64(os, m_id);
	os << serializeString32(m_design.write());
	os << serializeString32(m_built.write());
	return os.str();
}

void Object::renew()
{
	m_id = freshId();
}

void Object::build()
{
	m_built.read(m_design.write());
}

Piece Object::unbuilt() const
{
	Piece out(m_gamedef);
	m_design.each([&](v3s16 p, MapNode n, const shape::Shape *designed) {
		const MapNode b = m_built.node(p);
		if (b.getContent() != n.getContent()) {
			out.map().setNode(p, n);
			if (designed)
				out.map().setShape(p, *designed);
			return;
		}
		const shape::Shape *built = m_built.shape(p);
		if (!built)
			return; // built whole: all of it
		shape::Shape left;
		bool any = false;
		const shape::Shape want = designed ? *designed : shape::Shape::whole();
		for (s16 x = 0; x < shape::STEPS; x++)
			for (s16 y = 0; y < shape::STEPS; y++)
				for (s16 z = 0; z < shape::STEPS; z++)
					if (want.has({ x, y, z }) && !built->has({ x, y, z })) {
						left.set({ x, y, z }, true);
						any = true;
					}
		if (any) {
			out.map().setNode(p, n);
			out.map().setShape(p, left);
		}
	});
	return out;
}

} // namespace piece
