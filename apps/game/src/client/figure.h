// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_v3d.h"
#include "physics/skeleton.h"
#include <ISceneNode.h>
#include <SColor.h>
#include <functional>
#include <optional>
#include <vector>

namespace scene { class IMeshSceneNode; class ISceneManager; }
namespace video { class ITexture; }

/*
	A player's figure (sprint 58, visual "body"; jointed in sprint 115): a body
	of blocks in a person's proportions. Hips and chest with a waist between,
	head, upper arms and forearms with block hands, thighs and shins. It only
	shows physics::Poser's joints: movement is the animation.
*/
class Figure
{
public:
	// Under `parent` (the entity's frame: +z forward, feet at 0), its boxes
	// textured `white` and coloured, each made a material by `material`.
	Figure(scene::ISceneManager *smgr, scene::ISceneNode *parent, video::ITexture *white,
			const std::function<void(scene::ISceneNode *)> &material);
	~Figure();
	Figure(const Figure &) = delete;
	Figure &operator=(const Figure &) = delete;

	// What the entity's drawn as: hiding it hides the figure.
	scene::ISceneNode *root() const { return m_root; }

	// The ground's height against the feet at a place in the figure's frame (nodes).
	using Ground = std::function<float(v3f)>;
	// What keeps a ball at a place in the figure's frame out of solid things: how far to move it (nodes).
	using Clear = std::function<v3f(v3f, float)>;
	// (The head looks along the view exactly, whatever the body under it is doing.)
	// Posed after `dtime` moving at `velocity` (nodes/s, the figure's own
	// frame: +z ahead), looking `pitch` (radians, + down), in the air or not,
	// held as `stance`, the view having gone `turned` (radians) right; `face`: on a wall, its hips that far from the view;
	// `tilt`: leaned over sideways (radians, + to its left). `grip`: its hands on something, in the figure's frame (nodes).
	void pose(float dtime, v3f velocity, float pitch, bool airborne, physics::Stance stance, float turned, std::optional<float> face, float tilt, const physics::Contacts &contacts, const Ground &ground, const Clear &clear);
	// Lit as its place is (the entity's light).
	void light(video::SColor colour);
	// Seen from inside its own head: no head, the rest as it is, so you see your hands and feet.
	void hideHead(bool hide);
	// Where the head is, in the scene.
	v3f headAt() const;

private:
	struct Limb
	{
		scene::ISceneNode *pivot;   // turns at the joint
		scene::IMeshSceneNode *box;
	};
	Limb limb(scene::ISceneNode *parent, v3f joint, v3f size, v3f centre, video::SColor colour);

	scene::ISceneManager *m_smgr;
	video::ITexture *m_white;
	std::function<void(scene::ISceneNode *)> m_material;
	scene::ISceneNode *m_root;
	scene::ISceneNode *m_lower; // hips and legs, which turn from the view
	Limb m_hips, m_chest, m_head;
	Limb m_thighs[2], m_shins[2], m_upper_arms[2], m_forearms[2], m_hands[2];
	std::vector<scene::IMeshSceneNode *> m_boxes;
	physics::Poser m_poser;
	physics::Skeleton m_skeleton;
	v3f m_clear; // how far the upper body is moved out of what it's beside
};
