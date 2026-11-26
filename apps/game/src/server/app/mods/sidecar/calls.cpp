#include "src/server/app/mods/sidecar/calls.h"
#include "src/server/app/mods/sidecar/msgpack.h"
#include "src/server/app/mods/sidecar/ops.h"

namespace sidecar {

namespace {
std::vector<Host::Command> commandsOf(const mp::Value &set);
void none(mp::Writer &w) { w.nil(); }
}

Calls::Calls(Opts o) :
	m_host(o.host)
{}

std::function<void(mp::Writer &)> Calls::answer(const mp::Value &message)
{
	const auto &args = message.asArray();
	if (args.empty())
		throw mp::Error("a call names what it calls");
	const s64 call = args[0].asInt();
	auto arg = [&](size_t i) -> const mp::Value & {
		if (i + 1 >= args.size())
			throw mp::Error("call " + std::to_string(call) + " needs argument " + std::to_string(i));
		return args[i + 1];
	};
	switch ((Call)call) {
	case Call::PlayerTell:
		m_host.tell(arg(0).asString(), arg(1).asString());
		return none;
	case Call::GameAnnounce:
		m_host.announce(arg(0).asString());
		return none;
	case Call::CommandsSet:
		m_host.commands(commandsOf(arg(0)));
		return none;
	case Call::PlayerCan: {
		const bool may = m_host.can(arg(0).asString(), arg(1).asString());
		return [may](mp::Writer &w) { w.boolean(may); };
	}
	case Call::PlayerKick:
		m_host.kick(arg(0).asString(), arg(1).asString());
		return none;
	case Call::PlayersList: {
		std::vector<std::string> names = m_host.players();
		return [names = std::move(names)](mp::Writer &w) {
			w.array((u32)names.size());
			for (const std::string &n : names)
				w.string(n);
		};
	}
	case Call::PlayerPosition: {
		const std::optional<v3f> at = m_host.position(arg(0).asString());
		return [at](mp::Writer &w) {
			if (!at)
				return w.nil();
			w.array(3);
			w.number(at->X);
			w.number(at->Y);
			w.number(at->Z);
		};
	}
	case Call::TimeSkip: {
		const double seconds = arg(0).asNumber();
		if (!(seconds > 0) || seconds > 1e9)
			throw mp::Error("a skip is a number of seconds forward");
		m_host.skip((u32)seconds);
		return none;
	}
	case Call::WorldTime: {
		const v3f at((f32)arg(0).asNumber(), (f32)arg(1).asNumber(), (f32)arg(2).asNumber());
		std::optional<WorldClock::Reading> r = m_host.time(at);
		return [r = std::move(r)](mp::Writer &w) {
			if (!r)
				return w.nil();
			w.array(7);
			w.string(r->body);
			w.integer(r->year);
			w.integer(r->day);
			w.number(r->hour);
			w.number(r->daylight);
			w.string(r->season);
			w.number(r->day_seconds);
		};
	}
	}
	throw mp::Error("no call " + std::to_string(call));
}

namespace {

// [[path, description, usage, permission, [[arg, type, optional, options, description, suggest], …]], …]
std::vector<Host::Command> commandsOf(const mp::Value &set)
{
	std::vector<Host::Command> commands;
	for (const mp::Value &c : set.asArray()) {
		const auto &parts = c.asArray();
		if (parts.size() != 5)
			throw mp::Error("a command is [path, description, usage, permission, args]");
		Host::Command command{ parts[0].asString(), parts[1].asString(), parts[2].asString(),
				parts[3].asString(), {} };
		for (const mp::Value &a : parts[4].asArray()) {
			const auto &fields = a.asArray();
			if (fields.size() != 6)
				throw mp::Error("a command's argument is [name, type, optional, options, description, suggest]");
			Host::Command::Arg made{ fields[0].asString(), fields[1].asString(), fields[2].asBool(), {},
					fields[4].asString(), {} };
			for (const mp::Value &o : fields[3].asArray())
				made.options.push_back(o.asString());
			for (const mp::Value &o : fields[5].asArray()) {
				const auto &pair = o.asArray();
				if (pair.size() != 2)
					throw mp::Error("a suggestion is [value, description]");
				made.suggest.emplace_back(pair[0].asString(), pair[1].asString());
			}
			command.args.push_back(std::move(made));
		}
		commands.push_back(std::move(command));
	}
	return commands;
}

} // namespace

} // namespace sidecar
