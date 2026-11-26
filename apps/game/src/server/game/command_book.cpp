// Terraverse: every command a server answers. See command_book.h.
#include "src/server/game/command_book.h"

#include "src/server/app/server.h"
#include "src/server/world/state/environment/serverenvironment.h"
#include "src/server/game/commands.h"
#include "src/shared/game/objects/remoteplayer.h"
#include "src/shared/util/text/string.h"
#include "src/shared/base/constants.h"
#include <json/json.h>
#include <stdexcept>

namespace {

// a mod's command's first word: the engine routes by it, the mod finds the rest
std::string firstWord(const std::string &path)
{
	return path.substr(0, path.find(' '));
}

Json::Value argsJson(const std::vector<sidecar::Host::Command::Arg> &args)
{
	Json::Value out(Json::arrayValue);
	for (const sidecar::Host::Command::Arg &a : args) {
		Json::Value arg;
		arg["name"] = a.name;
		arg["type"] = a.type;
		arg["optional"] = a.optional;
		if (!a.options.empty()) {
			arg["options"] = Json::Value(Json::arrayValue);
			for (const std::string &o : a.options)
				arg["options"].append(o);
		}
		if (!a.description.empty())
			arg["description"] = a.description;
		if (!a.suggest.empty()) {
			arg["suggest"] = Json::Value(Json::arrayValue);
			for (const auto &[value, description] : a.suggest) {
				Json::Value s;
				s["value"] = value;
				if (!description.empty())
					s["description"] = description;
				arg["suggest"].append(s);
			}
		}
		out.append(arg);
	}
	return out;
}

} // namespace

std::string CommandBook::run(const std::string &name, RemotePlayer *player, const std::string &line)
{
	// the console (no player) may do anything: it's whoever runs the server
	const bool console = player == nullptr;
	const size_t space = line.find(' ');
	const std::string word = line.substr(0, space);
	const std::string rest = space == std::string::npos ? "" : trim(line.substr(space + 1));
	if (word == "help")
		return help(name, console);
	for (const EngineCommand &c : engineCommands()) {
		if (word != c.name)
			continue;
		if (!console && !may(name, c.permission))
			return std::string("You may not: /") + c.name + " needs " + c.permission + ".";
		try {
			return c.run(&m_opts.server, player, rest);
		} catch (const std::invalid_argument &e) {
			return e.what();
		}
	}
	for (const sidecar::Host::Command &c : m_mods) {
		if (word != firstWord(c.path))
			continue;
		// the mod answers, in its next window (it checks the path, the arguments and who may);
		// the console is "" to it
		if (!m_opts.server.sidecarCommand(console ? "" : name, word, rest))
			return "The mods are off, so nothing can answer /" + word + ".";
		return "";
	}
	return "There's no /" + word + ". /help lists what there is.";
}

void CommandBook::setMods(std::vector<sidecar::Host::Command> mods)
{
	m_mods = std::move(mods);
	for (RemotePlayer *player : m_opts.server.getEnv().getPlayers())
		if (player->getPeerId() != PEER_ID_INEXISTENT)
			tell(player);
}

void CommandBook::tell(RemotePlayer *player)
{
	const std::string &name = player->getName();
	Json::Value list(Json::arrayValue);
	for (const EngineCommand &c : engineCommands()) {
		if (!may(name, c.permission))
			continue;
		Json::Value command;
		command["name"] = c.name;
		command["usage"] = c.params;
		command["description"] = c.description;
		list.append(command);
	}
	for (const sidecar::Host::Command &c : m_mods) {
		if (!may(name, c.permission.empty() ? nullptr : c.permission.c_str()))
			continue;
		Json::Value command;
		command["name"] = c.path;
		command["usage"] = c.usage;
		command["description"] = c.description;
		command["args"] = argsJson(c.args);
		list.append(command);
	}
	Json::StreamWriterBuilder writer;
	writer["indentation"] = "";
	m_opts.server.setUi(player, "commands", Json::writeString(writer, list));
}

std::string CommandBook::help(const std::string &name, bool console)
{
	std::string out = "Commands:";
	for (const EngineCommand &c : engineCommands())
		if (console || may(name, c.permission))
			out += std::string("\n  /") + c.name + (*c.params ? " " : "") + c.params + " — " + c.description;
	for (const sidecar::Host::Command &c : m_mods)
		if (console || may(name, c.permission.empty() ? nullptr : c.permission.c_str()))
			out += "\n  /" + c.path + (c.usage.empty() ? "" : " ") + c.usage + " — " + c.description;
	return out;
}

bool CommandBook::may(const std::string &name, const char *permission)
{
	return !permission || m_opts.server.checkPriv(name, permission);
}
