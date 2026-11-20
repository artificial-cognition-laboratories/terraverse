// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/calendar.h"

namespace terragen {

const char *const SEASON_NAMES[(int)Season::Count] = {
	"spring", "summer", "autumn", "winter",
};

Calendar calendar(int32_t year_days, uint32_t day_count, fix time_of_day)
{
	uint32_t days = (uint32_t)year_days;
	uint32_t day = day_count % days;
	fix date = (fix)(((int64_t)day * ONE + time_of_day) / days);
	return { day_count / days, day, date };
}

Season season(fix date, fix latitude)
{
	if (latitude < 0)
		date = fraction(date + HALF);
	return (Season)floorInt(date * 4);
}

} // namespace terragen
