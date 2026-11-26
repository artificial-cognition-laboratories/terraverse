// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/world/terragen/src/base/fixed.h"

/*
	The calendar: where the world is in its year, from the engine's clock
	(the day count and the time of day it already keeps and saves). Day 0 is
	the northern spring equinox, as in the preview.
*/
namespace terragen {

enum class Season : uint8_t { Spring, Summer, Autumn, Winter, Count };

extern const char *const SEASON_NAMES[(int)Season::Count];

struct Calendar
{
	uint32_t year;  // years passed, from 0
	uint32_t day;   // day of the year, from 0
	fix date;       // 0..1 through the year, the time of day included
};

// year_days from the world code; time_of_day 0..1.
Calendar calendar(int32_t year_days, uint32_t day_count, fix time_of_day);

// The season at a latitude (radians): the south's are the north's, shifted
// half a year. The equator follows the north.
Season season(fix date, fix latitude);

} // namespace terragen
