// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/world/terragen/src/grid/drainage.h"
#include "library/world/terragen/src/base/fixed.h"
#include <cstdint>
#include <vector>

/*
	What wears the land, a round at a time, on the grid's cells: rivers by
	stream power, hillsides by creep. Heights are relative to the sea, which
	neither touches. The models are the published ones, checked against
	fastscapelib's by apps/game/library/world/terragen/tools/oracle.cpp.
*/
namespace terragen {

/*
	Stream power, E = K·A^½·S (m = ½, n = 1), solved implicitly from the sea
	upstream (Braun & Willett 2013): h ← (h + F·h_down) / (1 + F), F =
	K·√A ÷ d, d the distance to the cell it drains to in the grid's mean
	spacings. Land never wears below the sea. `area` is rain-weighted
	(accumulate), `k` each cell's erodibility for the round. With `power`,
	each cell's F × its fall after the round, which is what its stream can
	carry.

	A lake is its rivers' base level (Lakes::Base): a stream wears down to the
	lake's surface, not its floor, and nothing under the lake wears. Lakes::Bed
	is fastscapelib's convention instead (streams wear toward the bed they
	drain to; only cells draining uphill, out over a pass, are spared), for
	checking this solver against it (oracle.cpp).
*/
enum class Lakes { Base, Bed };
void streamPower(std::vector<fix> &h, const Routing &route, const std::vector<int64_t> &area,
		const std::vector<fix> &k, std::vector<fix> *power = nullptr, Lakes lakes = Lakes::Base);

/*
	Hillslope creep, κ∇²h, explicit: each land cell trades `kappa` of its
	height difference with each land neighbour (stable below 1/6). What's
	moved is conserved exactly: each cell's share of a unit of height it
	couldn't take is kept in `crumbs` (height × area) for the next round.
*/
void creep(std::vector<fix> &h, fix kappa, std::vector<int64_t> &crumbs);

/*
	Isostasy: the crust floats, so land worn away rises back by the share of
	it the mantle's weight makes up for (Airy: ρ_crust / ρ_mantle, about
	0.82), and land loaded with sediment sinks. The plate is stiff, so the
	response is spread over its flexural wavelength: `passes` rounds of
	neighbour averaging (Landlab's Flexure is the full thin-plate model).
	Valleys cut into a range lift its peaks; old land wears down slowly.
	Only the land moves (the sea, and where it ends, stay put): what's taken
	off since `before` is answered, and never below the sea. Returns the
	rock raised (height × area), negative where it sank.
*/
int64_t rebound(std::vector<fix> &h, const std::vector<fix> &before, fix share, int passes);

} // namespace terragen
