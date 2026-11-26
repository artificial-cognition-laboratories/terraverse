// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/matter/matter/matter.h"
#include <optional>
#include <string>
#include <utility>
#include <vector>

// What a block says it's made of (ContentFeatures, server only, sprint 119): the
// engine registers it with the world's matter (server/matter.cpp) and with
// machines (workshop::Materials) once the mods have declared their blocks.

// A block's matter (ContentFeatures::matter): what it's made of and how.
struct MatterBlock
{
	std::string matter;
	double share = 1;
	matter::State state = matter::State::Solid;
	bool form = false;     // the block its matter becomes
	bool flowing = false;  // a liquid the engine runs, not the world's matter
	bool vessel = false;   // air that keeps what's put in it (sprint 79)
	bool loose = false;    // a heap that slides (sprint 80)
};

// A family: param2 picks which of its compositions a block is.
struct MatterFamily
{
	double share = 1;
	matter::State state = matter::State::Solid;
	bool form = false; // its first point is the form its matter takes
	std::vector<std::vector<std::pair<std::string, double>>> points; // each { substance = share }
	std::vector<double> holds; // sprint 111: how much of each point's pores water fills; empty: none
};

// What it does in a machine (sprint 57, 69, 78): each set overrides what its matter
// says. Mass is kg in its block; strengths in Pa; power in kW.
struct MatterMachine
{
	std::optional<float> mass, friction, tension, compression, toughness, power, rpm;
	std::string role;
};
