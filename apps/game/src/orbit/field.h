// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_v3d.h"
#include "orbit/flight.h"
#include <memory>
#include <optional>
#include <vector>

namespace planet { class Atlas; }

/*
	Gravity as a field (sprint 59): what pulls on something at a place in
	the map, in the map's own terms. The planet's pull toward its centre
	(weaker with height), the turning frame's pulls and its air, from each
	body's planet card (terragen::sphere at its radius), worked out in the
	planet's frame by Flight and given in the axes of the grid the place is
	in (planet::Atlas::frame).

	Those axes turn from place to place (up is the way out from the centre
	everywhere), so a velocity is only a velocity where it was measured:
	`carry` re-expresses one at a new place, as the same motion. That's what
	makes something thrown sideways fall around the planet rather than ride
	a grid's straight line.

	Without an atlas (a lab world, a flat map) it's the home card's pull
	straight down −y with height as y. Above the atlas's top it's charted
	from the column under it. In the seam between tiles there's no frame
	to give it in: it says so.
*/
namespace orbit {

class Field
{
public:
	// `bodies`: each of the atlas's bodies as flight sees it (terragen::planetOf), in its order
	Field(const planet::Atlas &atlas, const std::vector<terragen::Planet> &bodies);
	explicit Field(const terragen::Planet &flat);
	~Field();

	struct Here
	{
		v3f gravity;  // nodes/s² in the place's grid: the pull and the turning frame's
		float air;    // kg/m³
	};
	// At a storage position, moving at `vel` (nodes/s there): none outside
	// every grid.
	std::optional<Here> at(const v3d &pos, const v3f &vel) const;
	// The same motion, measured at `from`, as it is at `to` (both in one
	// grid). Unchanged when either is outside every grid.
	v3f carry(const v3d &from, const v3d &to, const v3f &vel) const;
	// The orbit something there moving so is on; none outside every grid.
	std::optional<Elements> elements(const v3d &pos, const v3f &vel) const;
	// The path it coasts on from there (Flight::path, out to five of its
	// body's radii): points in its body's frame, metres from the centre.
	std::optional<std::vector<Vec>> path(const v3d &pos, const v3f &vel, int points) const;

private:
	struct Local
	{
		const Flight *flight;
		Vec point;           // on the planet, m
		Vec axes[3];         // planet metres per storage node, along x, y, z
		Vec out(const v3f &v) const;   // storage → planet
		v3f in(const Vec &v) const;    // planet → storage
	};
	std::optional<Local> local(const v3d &pos) const;

	const planet::Atlas *m_atlas = nullptr;
	std::vector<std::unique_ptr<Flight>> m_flights; // per body; one, flat
};

} // namespace orbit
