// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_v3d.h"
#include <vector>

/*
	Whether a shape holds itself up. Not the voxels: a coarse graph of cells
	(a quarter of a node, in the game) joined across the faces they share, each
	cell a rigid lump and each joint a spring with the joint's own cross-section.
	Weight goes in, each joint's axial force, shear and bending come out, and a
	joint past its material's strength breaks; then it's asked again, as what it
	carried has gone elsewhere. So a frame's load shares round its loops, and a thin
	neck fails where a thick one holds, as a player would guess.

	It's run when something comes to rest or takes a blow: nothing at rest or
	asleep costs anything. No engine dependencies, and no Jolt: what's touching comes in.
*/
namespace physics {

struct StructureCell
{
	v3f centre;       // nodes (metres)
	float mass;       // kg
	float tension;    // Pa it takes pulled apart or sheared; 0: it's never what fails
	float compression; // Pa it takes crushed
	float toughness = 0; // J/m² a crack through it costs: why metal dents where dirt shatters
};

// Two cells either side of a face, b the further along `axis`.
struct StructureBond
{
	unsigned a, b;
	unsigned char axis; // 0, 1, 2: X, Y, Z
	float length;       // between the cells' centres, m
	float area;         // of what's filled on both sides, m²
	float bend[2];      // its second moments about the other two axes, in order, m⁴
	float reach[2];     // how far its furthest fibre is from its middle along those two axes, m
};

struct Structure
{
	std::vector<StructureCell> cells;
	std::vector<StructureBond> bonds;
};

// What each cell's mass is pulled by: gravity, and through a blow what stopping
// asks of it, about the cells' middle.
struct StructureLoad
{
	v3f acceleration;      // m/s², the cells' frame
	v3f spin{ 0, 0, 0 };   // rad/s²
	std::vector<v3f> each; // none, or a cell's own besides (m/s²): a blast's near side
	// J it can spend on cracks: a blow only has so much. 0: its own weight, with no end to it
	float energy = 0;
	StructureLoad(v3f a, v3f s = v3f(0, 0, 0)) : acceleration(a), spin(s) {}
};

struct StructureResult
{
	std::vector<unsigned> group; // each cell's piece; 0 is the largest by mass
	unsigned groups = 1;
	unsigned rounds = 0;         // how many times it was solved
	unsigned broken = 0;         // joints that failed
	float worst = 0;             // the first solve's worst load, as a share of strength
	bool settled = true;         // false: a solve ran out of its budget, and nothing's broken on its word
};

// `resting[i]`: cell i is held by what it's touching.
StructureResult solveStructure(const Structure &s, const std::vector<bool> &resting, const StructureLoad &load);

} // namespace physics
