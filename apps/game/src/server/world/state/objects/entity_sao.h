// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2013-2020 Luanti core developers & community

#pragma once

#include "src/server/world/state/objects/unit_sao.h"
#include "src/shared/util/guid.h"
#include "src/server/mods/kinds.h"
#include <memory>

class EntitySAO : public UnitSAO
{
public:
	EntitySAO() = delete;
	// Used by the environment to load SAO
	EntitySAO(ServerEnvironment *env, v3f pos, const std::string &data);
	// A new one, of the kind `name` (server/kinds.h), from its state
	EntitySAO(ServerEnvironment *env, v3f pos, const std::string &name,
			const std::string &state);
	~EntitySAO();

	ActiveObjectType getType() const { return ACTIVEOBJECT_TYPE_LUAENTITY; }
	ActiveObjectType getSendType() const { return ACTIVEOBJECT_TYPE_GENERIC; }
	virtual void addedToEnvironment(u32 dtime_s);
	void step(float dtime, bool send_recommended);
	std::string getClientInitializationData(u16 protocol_version);

	bool isStaticAllowed() const { return m_prop.static_save; }
	bool shouldUnload() const { return true; }
	void getStaticData(std::string *result) const;

	u32 punch(v3f dir, const ToolCapabilities &toolcap,
			ServerActiveObject *puncher = nullptr,
			float time_from_last_punch = 1000000.0f,
			u16 initial_wear = 0);

	void rightClick(ServerActiveObject *clicker);

	void setPos(const v3f &pos);
	void moveTo(v3f pos, bool continuous);
	float getMinimumSavedMovement();

	std::string getDescription();

	void setHP(s32 hp, const PlayerHPChangeReason &reason);
	u16 getHP() const;
	std::string getGUID() const;

	/* EntitySAO-specific */
	void setVelocity(v3f velocity);
	void addVelocity(v3f velocity) { m_velocity += velocity; }
	v3f getVelocity();
	void setAcceleration(v3f acceleration);
	v3f getAcceleration();

	void setTextureMod(const std::string &mod);
	std::string getTextureMod() const;
	void setSprite(v2s16 p, int num_frames, float framelength,
			bool select_horiz_by_yawpitch);
	std::string getName();
	bool getCollisionBox(aabb3f *toset) const;
	bool getSelectionBox(aabb3f *toset) const;
	bool collideWithObjects() const;

	// Terra Firma (sprint 57): moved by the workshop's physics, not by its
	// own velocity and collisions; its rigid groups' poses sent to clients
	void setDriven(bool driven) { m_driven = driven; }
	bool isDriven() const { return m_driven; }
	void sendParts(const std::string &data);

	// Terra Firma (sprint 119): what it does (server/kinds.h); null for an entity of no
	// kind the engine knows, kept inert as it was saved
	EntityKind *kind() { return m_kind.get(); }
	const ObjectProperties &properties() const { return m_prop; }

private:
	// Terra Firma: on a planet, moved off its face from `was`: put on the face
	// it's moved onto, its velocity, acceleration and yaw turned with the grid.
	void carry(const v3f &was);
	bool m_driven = false; // Terra Firma

	std::string getPropertyPacket();
	void sendPosition(bool do_interpolate, bool is_movement_end);
	std::string generateSetTextureModCommand() const;
	static std::string generateSetSpriteCommand(v2s16 p, u16 num_frames,
			f32 framelength, bool select_horiz_by_yawpitch);

	std::string m_init_name;
	std::string m_init_state;
	std::unique_ptr<EntityKind> m_kind;

	MyGUID m_guid;

	v3f m_velocity;
	v3f m_acceleration;

	v3f m_last_sent_position;
	v3f m_last_sent_velocity;
	v3f m_last_sent_rotation;
	float m_last_sent_position_timer = 0.0f;
	float m_last_sent_move_precision = 0.0f;

	std::string m_texture_modifier;
	bool m_texture_modifier_sent = false;
};
