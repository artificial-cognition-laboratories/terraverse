// TypeScript mods, in a process of their own (sprint 119).
//
// `terra runtime <server folder>` loads every mod's server half from .terra/ and sends
// the content they declared. Then each server step opens a window: the step's events go
// over, mod code runs, and until the sidecar says it's idle the engine answers its calls.
// Mod code only runs inside a window, so what it reads of the world is synchronous and
// in order with what it wrote. A sidecar that dies is restarted; the server runs on.
#pragma once

#include "src/server/app/mods/sidecar/content.h"
#include "src/server/world/clock.h"
#include "src/shared/base/irr_v3d.h"
#include "src/shared/base/irrlichttypes.h"
#include <array>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace sidecar {

namespace mp { class Value; }

struct Error : std::runtime_error {
	using std::runtime_error::runtime_error;
};

// What mods can ask of the server. Called inside a window, on the server thread, with
// the environment locked.
struct Host {
	std::function<void(const std::string &player, const std::string &text)> tell;
	std::function<void(const std::string &text)> announce;
	std::function<std::vector<std::string>()> players;
	// Every command mods have now, by its whole path ("kiln light"): the whole set, replacing
	// the last. `usage` as chat shows it ("<to> <text>"); `permission` "" when anyone may.
	struct Command {
		// `options` restrict what's typed; `suggest` only offers, each [value, what it means]
		struct Arg {
			std::string name, type;
			bool optional;
			std::vector<std::string> options;
			std::string description;
			std::vector<std::pair<std::string, std::string>> suggest;
		};
		std::string path, description, usage, permission;
		std::vector<Arg> args;
	};
	std::function<void(const std::vector<Command> &commands)> commands;
	// Whether a player holds a permission ("kilns:light").
	std::function<bool(const std::string &player, const std::string &permission)> can;
	std::function<void(const std::string &player, const std::string &reason)> kick;
	// Where a player is, in blocks; none before they're placed.
	std::function<std::optional<v3f>(const std::string &player)> position;
	// The universe's clock (server/world/clock.h): a place's time, and skipping forward.
	std::function<std::optional<WorldClock::Reading>(const v3f &nodes)> time;
	std::function<void(u32 seconds)> skip;
};

class Calls;

class Sidecar {
public:
	// Starts `terra runtime <folder>` and waits for its content. Throws Error if the
	// mods can't load: a universe with holes in it doesn't start.
	Sidecar(std::string terra, std::string folder, Host host);
	~Sidecar();

	const Content &content() const { return m_content; }

	// Events for the next window.
	void joined(const std::string &player);
	void left(const std::string &player);
	// Someone typed one of the mods' commands: its first word, and the rest of the line.
	// `player` is "" from the console. False when mods are off: nobody will answer.
	bool command(const std::string &player, const std::string &name, const std::string &text);

	// Open a window: the events since the last one, then mod code until it's idle.
	// `now` is game time in seconds. Also where it notices the mods were prepared again
	// (.terra/manifest.json rewritten) and reloads them.
	void step(float dtime, double now);

	// The mods now declare different content than the universe started with: blocks the
	// running engine can't have. The server should stop and be started again.
	bool needsRestart() const { return m_content_changed; }

private:
	struct Process;
	struct Happened { u8 kind; std::string player, command, text; };

	void start();
	void stop();
	void reload();
	void restart(const std::string &why);
	std::string manifestTime() const;
	void window(float dtime, double now);
	void handleCall(u32 id, const mp::Value &call);
	void handleLog(const mp::Value &line);

	std::string m_terra, m_folder;
	Host m_host;
	std::unique_ptr<Calls> m_calls;  // answers mod calls from m_host
	std::unique_ptr<Process> m_process;
	Content m_content;
	std::string m_content_bytes;  // as first sent: what a restart must match
	bool m_has_content = false;   // the first content came: everything after is compared to it
	std::vector<Happened> m_events;
	std::vector<double> m_deaths;  // when, in game time: too many and mods stay off
	double m_now = 0;
	bool m_off = false;
	bool m_content_changed = false;
	std::string m_manifest_time;
	std::chrono::steady_clock::time_point m_next_look;
};

} // namespace sidecar
