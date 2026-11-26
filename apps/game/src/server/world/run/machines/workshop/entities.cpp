// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/server/world/run/machines/workshop/entities.h"

#include "src/shared/game/objects/object_properties.h"
#include "src/server/world/run/machines/piece/header.h"
#include "src/server/world/run/machines/piece/machine.h"
#include "src/server/world/state/objects/entity_sao.h"
#include "src/server/players/player_sao.h"
#include "src/server/world/state/environment/serverenvironment.h"
#include "src/server/world/run/machines/workshop/workshop.h"
#include <memory>

namespace workshop {

namespace {

const char ENTITY[] = "core:piece";
// How far round a player a machine they worked on is looked for, to undo
// (it may have moved; past this it's out of their way).
constexpr float UNDO_RADIUS = 128;

} // namespace

EntitySAO &Entities::object(u16 id)
{
	ServerActiveObject *obj = m_env.getActiveObject(id);
	auto *entity = obj && !obj->isGone() ? dynamic_cast<EntitySAO *>(obj) : nullptr;
	if (!entity || entity->accessObjectProperties()->visual != OBJECTVISUAL_PIECE)
		throw Refused("That's not an object");
	return *entity;
}

EntitySAO *Entities::find(PlayerSAO &who, u64 machine)
{
	std::vector<ServerActiveObject *> near;
	m_env.getObjectsInsideRadius(near, who.getBasePosition(), UNDO_RADIUS * BS, [&](ServerActiveObject *obj) {
		auto *entity = dynamic_cast<EntitySAO *>(obj);
		if (!entity || obj->isGone() || entity->accessObjectProperties()->visual != OBJECTVISUAL_PIECE)
			return false;
		return piece::header(entity->accessObjectProperties()->piece).id == machine;
	});
	return near.empty() ? nullptr : dynamic_cast<EntitySAO *>(near.front());
}

piece::Machine Entities::read(EntitySAO &obj) const
{
	piece::Machine m(m_gamedef);
	m.read(obj.accessObjectProperties()->piece);
	return m;
}

void Entities::store(EntitySAO &obj, const piece::Machine &m)
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

EntitySAO &Entities::spawn(v3f at, v3f rotation, const piece::Machine &m)
{
	auto obj = std::make_unique<EntitySAO>(&m_env, at, ENTITY, m.write());
	EntitySAO &out = *obj;
	out.setRotation(rotation);
	if (m_env.addActiveObject(std::move(obj)) == 0)
		throw Refused("The object couldn't be made there");
	return out;
}

} // namespace workshop
