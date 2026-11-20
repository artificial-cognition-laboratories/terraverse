// Terra Firma: the engine's own chat commands (sprint 119). See commands.h.
#include "src/server/mods/commands.h"

#include "src/server/players/player_sao.h"
#include "src/shared/objects/remoteplayer.h"
#include "src/server/app/server.h"
#include "src/server/world/terra.h"
#include <cmath>
#include <cstdio>
#include <sstream>
#include <stdexcept>

namespace {

// Where they stand; a console has nowhere to stand.
v3f standing(RemotePlayer *player)
{
	PlayerSAO *sao = player ? player->getPlayerSAO() : nullptr;
	if (!sao)
		throw std::invalid_argument("you're not in the world");
	return sao->getBasePosition() / BS;
}

std::vector<std::string> words(const std::string &args)
{
	std::istringstream in(args);
	std::vector<std::string> out;
	for (std::string w; in >> w;)
		out.push_back(w);
	return out;
}

// A number typed, or what it should have been.
double number(const std::string &word, const char *what)
{
	char *end = nullptr;
	const double v = std::strtod(word.c_str(), &end);
	if (word.empty() || *end != '\0' || !std::isfinite(v))
		throw std::invalid_argument(std::string(what) + " is a number, not " + word);
	return v;
}

// /travel: to another body, behind the travel screen. An admin's, as teleporting is: getting
// between bodies in play is flight's, not a command's.
std::string travelCommand(Server *server, RemotePlayer *player, const std::string &args)
{
	standing(player);
	const std::vector<std::string> bodies = terraBodyIds(server);
	if (bodies.size() < 2)
		throw std::invalid_argument("this world has no other bodies");
	const std::vector<std::string> w = words(args);
	if (w.empty() || w.size() == 2 || w.size() > 3) {
		std::string list;
		for (const std::string &b : bodies)
			list += (list.empty() ? "" : ", ") + b;
		throw std::invalid_argument("travel to one of: " + list);
	}
	std::optional<std::pair<double, double>> near;
	if (w.size() == 3) {
		const double lat = number(w[1], "a latitude"), lon = number(w[2], "a longitude");
		if (std::fabs(lat) > 90)
			throw std::invalid_argument("a latitude is from −90 to 90 degrees");
		near = std::pair{ lat, lon };
	}
	const std::string why = terraTravel(server, player, w[0], near);
	if (!why.empty())
		throw std::invalid_argument(why);
	return "travelling to " + w[0];
}

// /river: what the river you stand in is doing (the weather fills them, sprint 115).
std::string riverCommand(Server *server, RemotePlayer *player, const std::string &)
{
	const v3f at = standing(player);
	if (!terraWorld(server))
		throw std::invalid_argument("this world has no rivers");
	return terraRiverLine(server, at).value_or("no river or lake here");
}

// /rivers: a storm or a drought over the rivers round you, in place of the weather, so you
// can watch them answer.
std::string riversCommand(Server *server, RemotePlayer *player, const std::string &args)
{
	const v3f at = standing(player);
	if (!terraWorld(server))
		throw std::invalid_argument("this world has no rivers");
	const double STORM = 8; // mm/h: a day of it is a big one (about 200 mm)
	const std::vector<std::string> w = words(args);
	if (w.size() == 1 && w[0] == "clear") {
		terraForceRivers(server, at, 0, 0, 0);
		return "the weather has the rivers again";
	}
	if (w.empty() || w.size() > 3 || (w[0] != "storm" && w[0] != "drought"))
		throw std::invalid_argument("/rivers storm|drought [<hours> [<km>]] or /rivers clear");
	const double hours = w.size() > 1 ? number(w[1], "hours") : 24;
	const double km = w.size() > 2 ? number(w[2], "km") : 200;
	if (hours <= 0 || km <= 0)
		throw std::invalid_argument("hours and km must be over 0");
	terraForceRivers(server, at, km, hours, w[0] == "storm" ? STORM : 0);
	char line[160];
	std::snprintf(line, sizeof line, "a %s over %g km for %g h: the rivers answer at the weather's next step (3 h)",
			w[0].c_str(), km, hours);
	return line;
}

} // namespace

const std::vector<EngineCommand> &engineCommands()
{
	static const std::vector<EngineCommand> commands = {
		{ "travel", "<body> [<latitude> <longitude>]",
			"Go to another body of the world (degrees; its equator when no place is given)",
			"core:server", travelCommand },
		{ "river", "", "What the river you stand in is doing: its flow, against its usual, its level and speed",
			nullptr, riverCommand },
		{ "rivers", "storm|drought [<hours> [<km>]] | clear",
			"Bring a storm or a drought over the rivers round you, in place of the weather (default 24 h, 200 km)",
			"core:server", riversCommand },
	};
	return commands;
}
