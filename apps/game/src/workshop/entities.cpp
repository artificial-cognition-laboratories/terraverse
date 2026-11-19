// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "entities.h"

#include "object_properties.h"
#include "piece/header.h"
#include "piece/machine.h"
#include "server/luaentity_sao.h"
#include "server/player_sao.h"
#include "serverenvironment.h"
#include "workshop.h"
#include <memory>

namespace workshop {

namespace {

const char ENTITY[] = "core:piece";
// How far round a player a machine they worked on is looked for, to undo
// (it may have moved; past this it's out of their way).
constexpr float UNDO_RADIUS = 128;

} // namespace

LuaEntitySAO &Entities::object(u16 id)
{
	ServerActiveObject *obj = m_env.getActiveObject(id);
	auto *entity = obj && !obj->isGone() ? dynamic_cast<LuaEntitySAO *>(obj) : nullptr;
	if (!entity || entity->accessObjectProperties()->visual != OBJECTVISUAL_PIECE)
		throw Refused("That's not an object");
	return *entity;
}

LuaEntitySAO *Entities::find(PlayerSAO &who, u64 machine)
{
	std::vector<ServerActiveObject *> near;
	m_env.getObjectsInsideRadius(near, who.getBasePosition(), UNDO_RADIUS * BS, [&](ServerActiveObject *obj) {
		auto *entity = dynamic_cast<LuaEntitySAO *>(obj);
		if (!entity || obj->isGone() || entity->accessObjectProperties()->visual != OBJECTVISUAL_PIECE)
			return false;
		return piece::header(entity->accessObjectProperties()->piece).id == machine;
	});
	return near.empty() ? nullptr : dynamic_cast<LuaEntitySAO *>(near.front());
}

piece::Machine Entities::read(LuaEntitySAO &obj) const
{
	piece::Machine m(m_gamedef);
	m.read(obj.accessObjectProperties()->piece);
	return m;
}

void Entities::store(LuaEntitySAO &obj, const piece::Machine &m)
{
	if (m.empty()) {
		obj.markForRemoval();
		return;
	}
	ObjectProperties *props = obj.accessObjectProperties();
	props->piece = m.write();
	props->validate();
	obj.notifyObjectPropertiesModified();
}

LuaEntitySAO &Entities::spawn(v3f at, v3f rotation, const piece::Machine &m)
{
	auto obj = std::make_unique<LuaEntitySAO>(&m_env, at, ENTITY, m.write());
	LuaEntitySAO &out = *obj;
	out.setRotation(rotation);
	if (m_env.addActiveObject(std::move(obj)) == 0)
		throw Refused("The object couldn't be made there");
	return out;
}

} // namespace workshop
