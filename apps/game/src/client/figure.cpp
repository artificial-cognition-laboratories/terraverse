// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "figure.h"

#include "client/mesh.h"
#include "constants.h"
#include <cmath>
#include <IAnimatedMesh.h>
#include <IMeshSceneNode.h>
#include <ISceneManager.h>

namespace {

constexpr float S = physics::FIGURE_SCALE;

const video::SColor SKIN(255, 216, 160, 122), SHIRT(255, 0, 148, 210), TROUSERS(255, 58, 63, 85);

// A bone hanging `length` from its joint, wide as given
v3f hangingSize(float length, float wide) { return v3f(wide, length + physics::BONE_OVERLAP, wide); }
v3f hangingCentre(float length) { return v3f(0, (physics::BONE_OVERLAP - length) / 2, 0); }

} // namespace

Figure::Figure(scene::ISceneManager *smgr, scene::ISceneNode *parent, video::ITexture *white,
		const std::function<void(scene::ISceneNode *)> &material) :
	m_smgr(smgr), m_white(white), m_material(material)
{
	using namespace physics;
	m_root = smgr->addEmptySceneNode(parent);
	m_root->grab();
	// the chest turns at the waist; the head and arms go with it. The legs hang
	// from the root, not the hips: where a foot goes is worked out from there.
	m_lower = smgr->addEmptySceneNode(m_root);
	m_hips = limb(m_lower, v3f(0, HIP_HEIGHT, 0), v3f(0.48f, HIPS_TALL, 0.23f), v3f(0, HIPS_TALL / 2, 0), TROUSERS);
	m_chest = limb(m_hips.pivot, v3f(0, HIPS_TALL, 0), v3f(0.5f, CHEST_TALL + 0.02f, BONE_WIDE), v3f(0, CHEST_TALL / 2, 0), SHIRT);
	m_head = limb(m_chest.pivot, v3f(0, CHEST_TALL, 0), v3f(HEAD_SIZE), v3f(0, HEAD_SIZE / 2, 0), SKIN);
	for (int side : { 0, 1 }) {
		const float x = side ? -1.f : 1.f;
		m_thighs[side] = limb(m_lower, v3f(HIP_WIDE * x, HIP_HEIGHT, 0), hangingSize(THIGH, BONE_WIDE), hangingCentre(THIGH), TROUSERS);
		m_shins[side] = limb(m_thighs[side].pivot, v3f(0, -THIGH, 0), hangingSize(SHIN, BONE_NARROW), hangingCentre(SHIN), TROUSERS);
		m_upper_arms[side] = limb(m_chest.pivot, v3f(SHOULDER_OUT * x, SHOULDER_UP, 0), hangingSize(UPPER_ARM, BONE_WIDE), hangingCentre(UPPER_ARM), SHIRT);
		m_forearms[side] = limb(m_upper_arms[side].pivot, v3f(0, -UPPER_ARM, 0), hangingSize(FOREARM, BONE_NARROW), hangingCentre(FOREARM), SHIRT);
		m_hands[side] = limb(m_forearms[side].pivot, v3f(0, -FOREARM, 0), v3f(BONE_WIDE, HAND_LONG, BONE_WIDE), v3f(0, -HAND_LONG / 2, 0), SKIN);
	}
}

Figure::~Figure()
{
	m_root->remove(); // its limbs with it
	m_root->drop();
}

Figure::Limb Figure::limb(scene::ISceneNode *parent, v3f joint, v3f size, v3f centre, video::SColor colour)
{
	scene::ISceneNode *pivot = m_smgr->addEmptySceneNode(parent);
	pivot->setPosition(joint * S * BS);
	scene::IAnimatedMesh *mesh = createCubeMesh(size * S * BS);
	setMeshColor(mesh, colour);
	scene::IMeshSceneNode *box = m_smgr->addMeshSceneNode(mesh, pivot);
	mesh->drop();
	box->setPosition(centre * S * BS);
	m_material(box);
	box->forEachMaterial([this](video::SMaterial &mat) { mat.setTexture(0, m_white); });
	m_boxes.push_back(box);
	return { pivot, box };
}

void Figure::pose(float dtime, v3f velocity, float pitch, bool airborne, physics::Stance stance, float turned, std::optional<float> face, float tilt, const physics::Contacts &contacts, const Ground &ground, const Clear &clear)
{
	using namespace physics;
	if (dtime <= 0)
		return;
	PoseIn in;
	in.moving = velocity;
	in.pitch = pitch;
	const bool climbing = stance == Stance::VAULT || stance == Stance::MANTLE || stance == Stance::HANG || stance == Stance::WALL_RUN || stance == Stance::DOWN;
	in.airborne = airborne && !climbing;
	in.stance = stance;
	in.push = contacts.push;
	const physics::Grip *grip = contacts.grip;
	if (grip)
		in.hand_moving = grip->moving;
	if (contacts.step_on) {
		in.steps = true;
		in.step_on = *contacts.step_on / S;
	}
	in.feet = contacts.feet;
	in.fallen = contacts.fallen;
	in.rising = contacts.rising;
	in.stagger = contacts.stagger;
	in.throw_side = contacts.throw_side;
	in.throw_back = contacts.throw_back;
	if (airborne) // (the ground under it: where it comes down)
		in.above = std::max(0.f, -ground(v3f(0)));
	in.turned = turned;
	if (face) {
		in.faces = true;
		in.face = *face;
	}
	for (int side : { 0, 1 }) {
		// asked in the figure's scale, as its feet are drawn; no point looking from the air
		const v3f foot = m_poser.footAt(side) * S;
		in.ground[side] = in.airborne || climbing ? 0 : ground(foot) / S;
	}
	const Joints j = m_poser.step(dtime, in);

	const Bones b = m_skeleton.place(dtime, j, tilt, grip, m_clear);
	// Its head and shoulders go round what they meet. Asked where they'd be without the last
	// answer, so being moved clear doesn't read as there being nothing to clear.
	const bool upright = stance != Stance::ROLL && stance != Stance::PRONE && stance != Stance::SLIDE && stance != Stance::DOWN;
	v3f wanted;
	if (upright) {
		const core::matrix4 chest = b.lower.matrix() * b.hips.matrix() * b.chest.matrix();
		const auto ball = [&](const core::matrix4 &m, v3f at, float r) {
			m.transformVect(at);
			const v3f by = clear(at - m_clear, r);
			if (by.getLengthSQ() > wanted.getLengthSQ())
				wanted = by;
		};
		ball(chest * b.head.matrix(), v3f(0, HEAD_SIZE / 2, 0) * S, 0.27f);
		for (int side : { 0, 1 })
			ball(chest, b.upper[side].at, 0.14f);
	}
	if (wanted.getLength() > 0.3f)
		wanted = wanted / wanted.getLength() * 0.3f;
	m_clear += (wanted - m_clear) * std::min(1.f, 18 * dtime);
	const auto put = [](const Limb &limb, const BonePlace &at) {
		limb.pivot->setPosition(at.at * BS);
		limb.pivot->setRotation(at.turn);
	};
	m_lower->setPosition(b.lower.at * BS);
	m_lower->setRotation(b.lower.turn);
	put(m_hips, b.hips);
	put(m_chest, b.chest);
	put(m_head, b.head);
	for (int side : { 0, 1 }) {
		put(m_thighs[side], b.thigh[side]);
		put(m_shins[side], b.shin[side]);
		put(m_upper_arms[side], b.upper[side]);
		put(m_forearms[side], b.fore[side]);
		put(m_hands[side], b.hand[side]);
	}
}

void Figure::light(video::SColor colour)
{
	for (scene::IMeshSceneNode *box : m_boxes)
		box->forEachMaterial([&](video::SMaterial &mat) { mat.ColorParam = colour; });
}

v3f Figure::headAt() const
{
	return m_head.box->getAbsolutePosition();
}

void Figure::hideHead(bool hide)
{
	m_head.box->setVisible(!hide);
}
