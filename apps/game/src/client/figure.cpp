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
	const Bones b = m_posing.pose(dtime, velocity, pitch, airborne, stance, turned, face, tilt, contacts, ground, clear);
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
