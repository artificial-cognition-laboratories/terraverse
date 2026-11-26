// The universe's clock, as mods read and move it (@terraverse/time): a place's local time on
// its body, and skipping forward. There's one clock, in game seconds; the sky's days run
// time_speed times faster than it, and each body turns at its own rate through them.
#pragma once

#include "src/shared/base/irrlichttypes.h"
#include "src/shared/base/irr_v3d.h"
#include <optional>
#include <string>

class Server;

class WorldClock {
public:
	struct Opts {
		Server &server;
	};
	explicit WorldClock(Opts o);

	struct Reading {
		std::string body;    // the system's id for the body the place is on
		u32 year, day;       // its calendar's, from 1
		double hour;         // 0–24, local: noon on one side, midnight on the other
		double daylight;     // 0–1: the share of today the sun is up here
		std::string season;  // here: the south's are the north's, half a year on
		double day_seconds;  // game seconds in one of its days; 0 when its sun never moves
	};

	// A place's time (blocks). None on seam, or in a world terra didn't make.
	std::optional<Reading> at(const v3f &nodes) const;

	// The whole universe forward, every body together: the date, so the sky, the weather and
	// the rivers follow, and game time, so what's timed by it comes due.
	void skip(u32 seconds);

private:
	Server &m_server;
};
