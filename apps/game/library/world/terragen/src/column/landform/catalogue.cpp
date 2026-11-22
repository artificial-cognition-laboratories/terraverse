// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/src/column/landform/landform.h"

namespace terragen {

namespace {

using S = Setting;
// (any rock, any rain, any warmth)
constexpr Band ROCK = { 0, ONE }, RAIN = { 0, toFix(4) }, WARMTH = { -toFix(60), toFix(60) };

} // namespace

// (name, settings, limestone, hard, rain, celsius, shape, wave, high, share)
const LandformRow LANDFORMS[(int)Landform::Count] = {
	// Inselbergs: hard granite domes left standing as a worn shield's plain
	// was stripped round them, in the seasonal tropics (Uluru, Sugarloaf,
	// the kopjes of the Serengeti, Zimbabwe's dwalas)
	{ "inselbergs", in(S::Craton, S::Swell, S::OldBelt, S::Suture), false, { ratio(40, 100), ONE },
			{ ratio(20, 100), ratio(150, 100) }, { toFix(10), toFix(45) }, Shape::Dome, 40, 14, ratio(14, 100) },
	// Mesas and buttes: flat beds, a hard cap over softer, cut back by dry
	// country's rare floods into tables and then pillars (Monument Valley,
	// the Deccan's flat-topped hills on its lavas)
	{ "mesas and buttes", in(S::Platform, S::FloodBasalt, S::Sag, S::FailedRift), false, ROCK,
			{ 0, ratio(70, 100) }, WARMTH, Shape::Table, 56, 10, ratio(35, 100) },
	// Cuestas: tilted beds worn into long ridges, a gentle slope up the dip
	// and a scarp down its edge, where a basin's beds rise to its rim (the
	// Cotswolds, the Paris basin's, Niagara's)
	{ "cuestas", in(S::Platform, S::Margin, S::Foreland), false, ROCK, RAIN, WARMTH, Shape::Scarp, 96, 8, ratio(60, 100) },
	// Tower karst: limestone dissolved down round towers in the wet tropics
	// (Guilin, Ha Long, the mogotes of Cuba)
	{ "tower karst", in(S::Platform, S::Sag, S::Margin, S::Foreland, S::FailedRift), true, ROCK,
			{ ratio(120, 100), toFix(4) }, { toFix(18), toFix(45) }, Shape::Tower, 14, 16, ratio(30, 100) },
	// Badlands: soft clay and silt beds in dry country, gullied by the few
	// storms faster than anything grows on them (the Dakota Badlands,
	// Cappadocia's, the Bisti)
	{ "badlands", in(S::Foreland, S::Sag, S::FailedRift, S::Rift), false, { 0, ratio(45, 100) },
			{ ratio(10, 100), ratio(60, 100) }, { 0, toFix(40) }, Shape::Ridged, 8, 5, ratio(50, 100) },
	// Cinder cones: fields of small volcanoes over a rift or a plume, each
	// a single eruption's (the Chaîne des Puys, Lava Beds, Auckland's)
	{ "cinder cones", in(S::Rift, S::Volcanic, S::FloodBasalt, S::Swell), false, ROCK, RAIN, WARMTH,
			Shape::Cone, 32, 10, ratio(8, 100) },
	// Drumlins: low whalebacks an ice sheet moulded its till into, in swarms
	// over the plains it crossed (Ireland's, upstate New York's), where it's
	// cold enough now that it was under ice then
	{ "drumlins", in(S::Craton, S::Platform, S::Sag, S::OldBelt, S::Margin), false, ROCK, RAIN,
			{ -toFix(15), toFix(6) }, Shape::Dome, 16, 3, ratio(30, 100) },
};

} // namespace terragen
