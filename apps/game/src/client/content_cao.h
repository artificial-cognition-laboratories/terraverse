// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

namespace piece { class Machine; }
class Figure;
#include "physics/walk.h" // Terra Firma: stances

#include "irrlichttypes.h"

#include <EMaterialTypes.h>
#include <IDummyTransformationSceneNode.h>
#include <AnimSpec.h>

#include "object_properties.h"
#include "clientobject.h"
#include "constants.h"
#include "itemgroup.h"
#include "client/tile.h"
#include <cassert>
#include <memory>
#include <memory>

namespace scene {
	class IMeshSceneNode;
	class IBillboardSceneNode;
	class AnimatedMeshSceneNode;
}

class Client;
struct Nametag;
struct MinimapMarker;
class WieldMeshSceneNode;

enum class LocalPlayerAnimation : u8;

/*
	SmoothTranslator and other helpers
*/

template<typename T>
struct SmoothTranslator
{
	T val_old;
	T val_current;
	T val_target;
	f32 anim_time = 0;
	f32 anim_time_counter = 0;
	bool aim_is_end = true;

	SmoothTranslator() = default;

	void init(T current);

	void update(T new_target, bool is_end_position = false,
		float update_interval = -1);

	void translate(f32 dtime);
};

struct SmoothTranslatorWrapped : SmoothTranslator<f32>
{
	void translate(f32 dtime);
};

struct SmoothTranslatorWrappedv3f : SmoothTranslator<v3f>
{
	void translate(f32 dtime);
};

struct MeshAnimationInfo {
	u32 i; /// index of mesh buffer
	int frame; /// last animation frame
	TileLayer tile;
};

/*
	GenericCAO
*/

class GenericCAO : public ClientActiveObject
{
private:
	// Terra Firma: on an atlas, moved over a seam from `was`: put in the grid
	// it's moved into, velocity, acceleration and yaw turned. Whether it was.
	bool carry(const v3f &was);


	// Only set at initialization
	std::string m_name = "";
	bool m_is_player = false;
	bool m_is_local_player = false;
	// Property-ish things
	ObjectProperties m_prop;
	//
	scene::ISceneManager *m_smgr = nullptr;
	Client *m_client = nullptr;
	aabb3f m_selection_box = aabb3f(-BS/3.,-BS/3.,-BS/3., BS/3.,BS/3.,BS/3.);

	// Visuals
	scene::IMeshSceneNode *m_meshnode = nullptr;
	scene::AnimatedMeshSceneNode *m_animated_meshnode = nullptr;
	WieldMeshSceneNode *m_wield_meshnode = nullptr;
	scene::IBillboardSceneNode *m_spritenode = nullptr;
	scene::IDummyTransformationSceneNode *m_matrixnode = nullptr;
	Nametag *m_nametag = nullptr;
	MinimapMarker *m_marker = nullptr;
	bool m_visuals_expired = false;
	video::SColor m_last_light = video::SColor(0xFFFFFFFF);
	bool m_is_visible = false;
	std::vector<MeshAnimationInfo> m_meshnode_animation;

	// Material
	video::E_MATERIAL_TYPE m_material_type = video::EMT_INVALID;

	// Movement
	v3f m_position = v3f(0.0f, 10.0f * BS, 0);
	v3f m_velocity;
	v3f m_acceleration;
	v3f m_rotation;
	std::unique_ptr<piece::Machine> m_machine; // Terra Firma
	std::unique_ptr<Figure> m_figure; // Terra Firma: visual "body" (sprint 58)
	v3f m_figure_was;    // where it was last frame (BS): how fast it moves
	v3f m_figure_moving; // nodes/s, smoothed
	float groundUnder(v3f at) const;
	physics::Stance m_figure_stance = physics::Stance::STAND; // others'; yours is your body's
	float m_figure_yaw = 0;  // degrees: the view's, last frame
	bool m_was_dig = false, m_was_place = false; // yours: clicks held last frame (each press throws a strike)
	// Terra Firma (sprint 57): its rigid groups but the first, each drawn on
	// its own (a wheel turns), and where each is in the first's (BS; x y z w)
	std::vector<scene::IMeshSceneNode *> m_group_nodes;
	struct GroupPose
	{
		v3f offset;
		f32 x, y, z, w;
	};
	// glided from where they were drawn to the server's latest over its
	// update interval, as the machine's position is (sprint 66: a wheel's
	// turn put at once while the body glides tore it off the axle)
	std::vector<GroupPose> m_group_poses, m_group_from;
	// each group's middle in its own frame (BS): glided, it's the middle
	// that moves straight (sprint 71: a wheel's origin is the machine's, so
	// gliding that cut a spinning wheel's middle through the ground)
	std::vector<v3f> m_group_middles;
	f32 m_group_t = 1, m_group_interval = 0.1f;
	// each group's turn from where it was drawn to the server's latest (an
	// axis times radians, the machine's frame), and how fast it was turning
	// (rad/s): a wheel turning more than half a turn between poses was
	// glided the short way, backwards, and wobbled (sprint 79), so the turn
	// taken is the one that carries on as it was going
	std::vector<v3f> m_group_turn, m_group_rate;
	GroupPose groupShown(size_t i) const;
	void poseGroups();
	// Terra Firma: how its parts are drawn
	struct Look
	{
		std::optional<u64> selected; // drawn tinted; the rest dimmed
		std::optional<u64> hovered;  // drawn as it is, the rest see-through, when looking through
		bool through = false;        // see-through (selecting): what's under the cursor drawn solid
		bool lit = false;            // all of it lit: pointed at in the world, a press grabs it
		bool operator==(const Look &o) const
		{
			return selected == o.selected && hovered == o.hovered && through == o.through && lit == o.lit;
		}
	};
	Look m_look;
	std::optional<std::pair<v3f, v3f>> m_posed;
public:
	// Terra Firma: where the server last said it is (BS), turned (degrees),
	// moving (BS/s): posed here, it's drawn elsewhere (sprint 68)
	struct Served
	{
		v3f position, rotation, velocity;
	};
private:
	Served m_served;
	u16 m_hp = 1;
	SmoothTranslator<v3f> pos_translator;
	SmoothTranslatorWrappedv3f rot_translator;

	// Spritesheet stuff
	// TODO move into own struct
	v2f m_tx_size = v2f(1,1);
	v2s16 m_tx_basepos;
	bool m_initial_tx_basepos_set = false;
	bool m_tx_select_horiz_by_yawpitch = false;
	int m_anim_frame = 0;
	int m_anim_num_frames = 1;
	float m_anim_framelength = 0.2f;
	float m_anim_timer = 0.0f;

	/// The animation for all tracks as specified by the server.
	/// This is usually what is used, unless overridden by a local player animation.
	scene::AnimSpec m_animation;
	/// For the local player CAO, animations may be overridden by the client
	/// based on the in-game state of the local player (e.g. walking, digging, idling).
	/// See also LocalPlayerAnimation (player.h), LocalPlayer::last_animation (localplayer.h).
	bool m_local_player_animation = false;
	/// Deferred set animation commands, to be run once the scene node exists
	std::vector<std::pair<scene::TrackId, scene::TrackAnimSpec>> deferred_set_animation_cmds;

	void applyTrackAnimation(scene::TrackId &&track_id, scene::TrackAnimSpec anim);

	// stores position and rotation for each bone name
	BoneOverrideMap m_bone_override;

	// Attachments
	object_t m_attachment_parent_id = 0;
	std::unordered_set<object_t> m_attachment_child_ids;
	std::string m_attachment_bone = "";
	v3f m_attachment_position;
	v3f m_attachment_rotation;
	bool m_attached_to_local = false;
	bool m_force_visible = false;

	ItemGroupList m_armor_groups;
	float m_reset_textures_timer = -1.0f;
	// stores texture modifier before punch update
	std::string m_previous_texture_modifier = "";
	// last applied texture modifier
	std::string m_current_texture_modifier = "";
	float m_step_distance_counter = 0.0f;

	bool visualExpiryRequired(const ObjectProperties &newprops) const;

public:

	GenericCAO(Client *client, ClientEnvironment *env);

	~GenericCAO();

	static std::unique_ptr<ClientActiveObject> create(Client *client, ClientEnvironment *env)
	{
		return std::make_unique<GenericCAO>(client, env);
	}

	inline ActiveObjectType getType() const override
	{
		return ACTIVEOBJECT_TYPE_GENERIC;
	}
	inline const ItemGroupList &getGroups() const
	{
		return m_armor_groups;
	}
	void initialize(const std::string &data) override;

	void processInitData(const std::string &data);

	bool getCollisionBox(aabb3f *toset) const override;

	bool collideWithObjects() const override;

	virtual bool getSelectionBox(aabb3f *toset) const override;

	const v3f getPosition() const override final;

	const v3f getVelocity() const override final { return m_velocity; }

	inline const v3f &getRotation() const { return m_rotation; }
	// Terra Firma: its piece's blocks, if it's a piece (visual "piece")
	// Terra Firma: the machine it carries (visual "piece"), if it's one
	const piece::Machine *getMachine() const { return m_machine.get(); }
	// Terra Firma: the part selected tinted, the rest dimmed; looking
	// through (selecting), all see-through but the part hovered and the
	// part selected
	void setLook(const Look &look);
	// Terra Firma: drawn where this client says (BS, and its rotation in
	// degrees), carried or held; none: where the server says
	void setPosed(std::optional<std::pair<v3f, v3f>> posed);
	// Terra Firma: posed so and drawn there now, not at its next step (what
	// the camera's already moved for this frame is drawn where it's seen)
	void applyPosed(std::pair<v3f, v3f> posed);
	const Served &served() const { return m_served; }
	// Terra Firma: its grid in the world as it's drawn (piece::frame)
	core::matrix4 getFrame() const;

	bool isImmortal() const;

	inline const ObjectProperties &getProperties() const { return m_prop; }

	inline const std::string &getName() const { return m_name; }

	scene::ISceneNode *getSceneNode() const override;

	scene::AnimatedMeshSceneNode *getAnimatedMeshSceneNode() const override;

	// m_matrixnode controls the position and rotation of the child node
	// for all scene nodes, as a workaround for an Irrlicht problem with
	// rotations. The child node's position can't be used because it's
	// rotated, and must remain as 0.
	// Note that m_matrixnode.setPosition() shouldn't be called. Use
	// m_matrixnode->getRelativeTransformationMatrix().setTranslation()
	// instead (aka getPosRotMatrix().setTranslation()).
	inline core::matrix4 &getPosRotMatrix()
	{
		assert(m_matrixnode);
		return m_matrixnode->getRelativeTransformationMatrix();
	}

	inline const core::matrix4 *getAbsolutePosRotMatrix() const
	{
		if (!m_matrixnode)
			return nullptr;
		return &m_matrixnode->getAbsoluteTransformation();
	}

	inline f32 getStepHeight() const
	{
		return m_prop.stepheight;
	}

	inline bool isLocalPlayer() const override
	{
		return m_is_local_player;
	}

	inline bool isPlayer() const
	{
		return m_is_player;
	}

	inline bool isVisible() const
	{
		return m_is_visible;
	}

	inline void setVisible(bool toset)
	{
		m_is_visible = toset;
	}

	void setChildrenVisible(bool toset);
	void setAttachment(object_t parent_id, const std::string &bone, v3f position,
			v3f rotation, bool force_visible) override;
	void getAttachment(object_t *parent_id, std::string *bone, v3f *position,
			v3f *rotation, bool *force_visible) const override;
	void clearChildAttachments() override;
	void addAttachmentChild(object_t child_id) override;
	void removeAttachmentChild(object_t child_id) override;
	ClientActiveObject *getParent() const override;
	const std::unordered_set<object_t> &getAttachmentChildIds() const override
	{ return m_attachment_child_ids; }
	void updateAttachments() override;

	void removeFromScene(bool permanent) override;

	void addToScene(ITextureSource *tsrc, scene::ISceneManager *smgr) override;

	inline void expireVisuals()
	{
		m_visuals_expired = true;
	}

	void updateLight(u32 day_night_ratio) override;

	void setNodeLight(const video::SColor &light);

	/* Get light position(s).
	 * returns number of positions written into pos[], which must have space
	 * for at least 3 vectors. */
	u16 getLightPosition(v3s16 *pos);

	void updateNametag();

	void updateMarker();

	void updateNodePos();

	void step(float dtime, ClientEnvironment *env) override;

	void updateTextureAnim();

	// ffs this HAS TO BE a string copy! See #5739 if you think otherwise
	// Reason: updateTextures(m_previous_texture_modifier);
	void updateTextures(std::string mod);

	void updateAnimation(u16 track_nr);
	void setLocalPlayerAnimation(LocalPlayerAnimation local_anim, float speed);

	/// @param lax if false, logs a warning for invalid IDs
	std::optional<u16> resolveTrackId(const scene::TrackId &id, bool lax = false);

	void processMessage(const std::string &data) override;

	bool directReportPunch(v3f dir, const ItemStack *punchitem,
			const ItemStack *hand_item, float time_from_last_punch=1000000) override;

	std::string debugInfoText() override;

	std::string infoText() override
	{
		return m_prop.infotext;
	}

	void updateMeshCulling();

private:

	/// Update the parent chain so getPosition() returns an up to date position.
	void updateParentChain() const;

};
