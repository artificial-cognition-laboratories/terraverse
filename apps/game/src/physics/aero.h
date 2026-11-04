// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_aabb3d.h"
#include "irr_v3d.h"
#include <vector>

/*
	What air does to a shape (sprint 103). Nothing is a wing by name: a body's
	boxes are sliced along each way across its front, and what's long and thin
	in a slice is a wing's section there. Where it slopes, how it's curved and
	how thick it is are read off the section; slices side by side make a wing,
	and how far it spans against its chord is how little it wastes. A fin is
	the same on its side.

	Everything also pushes back with the area it shows along each of its own
	axes, so a plate flat on is a parachute. Thin sections are taken out of the
	area shown ahead: they part the air, and pay in friction instead.
*/
namespace physics {

struct Surface
{
	v3f at;      // where it pushes: its quarter chord (nodes, the body's frame)
	v3f normal;  // unit: its positive side
	v3f chord;   // unit: toward its front
	float area;  // m²
	float incidence; // radians: its front above its back, toward `normal`
	float camber;    // of its chord: its middle above the line front to back
	float thickness; // of its chord
	float aspect;    // its wing's span² / area
};

struct Aero
{
	std::vector<Surface> surfaces;
	v3f shown;      // m²: its silhouette along x, y and z
	v3f centre[3];  // the middle of each silhouette (nodes, the body's frame)
};

// `ahead` a unit vector, taken as its nearest axis.
Aero aeroOf(const std::vector<aabb3f> &boxes, v3f ahead);

// A surface's coefficients with the air `alpha` radians onto it, its positive
// side lifted by a positive one. Lift is along that side, drag against the flow.
struct Coefficients
{
	float lift, drag;
};
Coefficients coefficients(const Surface &s, float alpha);

// Where a wing's lift acts as its angle changes (nodes, the body's frame): the
// surfaces' quarter chords, each by how strongly it answers. What a craft's
// centre of mass must sit ahead of to fly straight. False if it has none level.
bool centreOfLift(const std::vector<Aero> &bodies, v3f up, v3f &centre);

} // namespace physics
