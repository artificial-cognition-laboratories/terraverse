#include "src/server/world/clock.h"
#include "src/server/world/terra.h"
#include "src/server/app/server.h"
#include "src/server/world/state/environment/serverenvironment.h"
#include "library/world/planet/atlas.h"
#include "library/world/terragen/src/base/fixed.h"
#include "library/world/terragen/src/code/worlds.h"
#include "library/world/terragen/src/system/calendar.h"
#include <cmath>

namespace {
constexpr double SKY_DAY = 86400; // the engine's day, in the seconds time_speed is a rate of
constexpr u32 TIME_OF_DAY = 24000; // the engine's time of day, 0 to this
}

WorldClock::WorldClock(Opts o) :
	m_server(o.server)
{}

std::optional<WorldClock::Reading> WorldClock::at(const v3f &nodes) const
{
	const std::optional<TerraSun> sun = terraSun(&m_server, nodes);
	if (!sun)
		return std::nullopt;
	ServerEnvironment &env = m_server.getEnv();
	const int body = m_server.atlas()->bodyAt(planet::Atlas::moverNode(nodes.X, nodes.Y, nodes.Z))
			.value_or(planet::Atlas::HOME);
	const terragen::BodyWorld &world = m_server.bodyWorlds().at(body);
	const float time_of_day = env.getTimeOfDayF();
	const terragen::Calendar date = terragen::calendar(world.code.year, env.getDayCount(),
			(terragen::fix)(time_of_day * terragen::ONE));

	// the body's clock counts from the world's start (TerraSun): its days are its own turns
	const double turned = sun->at + sun->turns * (env.getDayCount() + (double)time_of_day);
	const double speed = m_server.getTimeSpeed();
	const bool moves = speed > 0 && sun->turns > 1e-9 && !terraFacingStar(&m_server, body);
	return Reading{
		world.id,
		date.year + 1, date.day + 1,
		(turned - std::floor(turned)) * 24,
		sun->daylight,
		sun->season,
		moves ? SKY_DAY / speed / sun->turns : 0,
	};
}

void WorldClock::skip(u32 seconds)
{
	ServerEnvironment &env = m_server.getEnv();
	// in the engine's ticks of the day, rounded once, so a skip to noon lands on noon
	const double days = env.getTimeOfDayF() + seconds * (double)m_server.getTimeSpeed() / SKY_DAY;
	const u64 ticks = (u64)std::llround(days * TIME_OF_DAY);
	m_server.setDate(env.getDayCount() + (u32)(ticks / TIME_OF_DAY), (u32)(ticks % TIME_OF_DAY));
	env.skipGameTime(seconds);
}
