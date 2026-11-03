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

// Its proportions (nodes) before it's scaled to the player's height: legs
// and torso three quarters of a node each, a head of half.
constexpr float TALL = 2.0f, HEIGHT = 1.77f, S = HEIGHT / TALL;
// Bones overlap at their joints, the lower one narrower, so a bend shows no gap
// and their faces don't fight.
constexpr float OVERLAP = 0.05f, WIDE = 0.25f, NARROW = 0.22f;
constexpr float HIPS = 0.25f, CHEST = 0.5f, HEAD = 0.5f, SHOULDER = 0.375f;
constexpr float HAND = 0.125f;

const video::SColor SKIN(255, 216, 160, 122), SHIRT(255, 0, 148, 210), TROUSERS(255, 58, 63, 85);

// A bone hanging `length` from its joint, wide as given
v3f hangingSize(float length, float wide) { return v3f(wide, length + OVERLAP, wide); }
v3f hangingCentre(float length) { return v3f(0, (OVERLAP - length) / 2, 0); }

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
	m_hips = limb(m_lower, v3f(0, HIP_HEIGHT, 0), v3f(0.48f, HIPS, 0.23f), v3f(0, HIPS / 2, 0), TROUSERS);
	m_chest = limb(m_hips.pivot, v3f(0, HIPS, 0), v3f(0.5f, CHEST + 0.02f, WIDE), v3f(0, CHEST / 2, 0), SHIRT);
	m_head = limb(m_chest.pivot, v3f(0, CHEST, 0), v3f(HEAD), v3f(0, HEAD / 2, 0), SKIN);
	for (int side : { 0, 1 }) {
		const float x = side ? -1.f : 1.f;
		m_thighs[side] = limb(m_lower, v3f(HIP_WIDE * x, HIP_HEIGHT, 0), hangingSize(THIGH, WIDE), hangingCentre(THIGH), TROUSERS);
		m_shins[side] = limb(m_thighs[side].pivot, v3f(0, -THIGH, 0), hangingSize(SHIN, NARROW), hangingCentre(SHIN), TROUSERS);
		m_upper_arms[side] = limb(m_chest.pivot, v3f(0.375f * x, SHOULDER, 0), hangingSize(UPPER_ARM, WIDE), hangingCentre(UPPER_ARM), SHIRT);
		m_forearms[side] = limb(m_upper_arms[side].pivot, v3f(0, -UPPER_ARM, 0), hangingSize(FOREARM, NARROW), hangingCentre(FOREARM), SHIRT);
		m_hands[side] = limb(m_forearms[side].pivot, v3f(0, -FOREARM, 0), v3f(WIDE, HAND, WIDE), v3f(0, -HAND / 2, 0), SKIN);
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

void Figure::pose(float dtime, v3f velocity, float pitch, bool airborne, physics::Stance stance, float turned, std::optional<float> face, float tilt, const Ground &ground)
{
	using namespace physics;
	if (dtime <= 0)
		return;
	PoseIn in;
	in.moving = velocity;
	in.pitch = pitch;
	const bool climbing = stance == Stance::VAULT || stance == Stance::MANTLE || stance == Stance::HANG || stance == Stance::WALL_RUN;
	in.airborne = airborne && !climbing;
	in.stance = stance;
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

	const float deg = core::RADTODEG;
	m_hips.pivot->setPosition(v3f(0, (HIP_HEIGHT - j.drop) * S * BS, 0));
	// Turned from the view, over in a roll, and into its shoulder first (read right to left).
	core::matrix4 twist, spin, lean, tipped;
	tipped.setRotationRadians(v3f(0, 0, tilt));
	twist.setRotationRadians(v3f(0, j.twist, 0));
	spin.setRotationRadians(v3f(j.spin, 0, 0));
	lean.setRotationRadians(v3f(0, j.spin_lean, 0));
	const core::matrix4 turn = tipped * twist * spin * lean;
	m_lower->setRotation(turn.getRotationDegrees());
	// a roll turns about the body's middle, not its feet
	const v3f middle(0, 0.45f, 0);
	const v3f moved = turn.rotateAndScaleVect(middle);
	const v3f backed = twist.rotateAndScaleVect(v3f(0, 0, -j.back));
	m_lower->setPosition((middle - moved + backed) * S * BS);
	m_hips.pivot->setRotation(v3f((j.pitch + j.waist * 0.3f) * deg, 0, 0));
	// the chest stays mostly with the hips; the head does the turning
	m_chest.pivot->setRotation(v3f(j.waist * 0.7f * deg, -j.twist * 0.2f * deg, 0));
	// The head's own turn is what's left between its parents and the view. Summing angles
	// isn't enough: on a bent body they don't add, and it rolled over sideways.
	core::matrix4 parents = m_lower->getRelativeTransformation() * m_hips.pivot->getRelativeTransformation() *
			m_chest.pivot->getRelativeTransformation();
	parents.setTranslation(v3f(0));
	core::matrix4 undo, view;
	parents.getInverse(undo);
	view.setRotationDegrees(v3f(j.head * deg, 0, 0));
	// (rolling, it tucks and goes round with the body)
	m_head.pivot->setRotation(j.spin != 0 ? v3f(0.5f * deg, 0, 0) : (undo * view).getRotationDegrees());
	for (int side : { 0, 1 }) {
		const float x = side ? -HIP_WIDE : HIP_WIDE;
		m_thighs[side].pivot->setPosition(v3f(x, HIP_HEIGHT - j.drop, 0) * S * BS);
		m_thighs[side].pivot->setRotation(v3f(j.hip[side] * deg, 0, j.hip_out[side] * deg));
		m_shins[side].pivot->setRotation(v3f(j.knee[side] * deg, 0, 0));
		m_upper_arms[side].pivot->setRotation(v3f(j.shoulder[side] * deg, 0, j.shoulder_out[side] * deg));
		m_forearms[side].pivot->setRotation(v3f(j.elbow[side] * deg, 0, 0));
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
