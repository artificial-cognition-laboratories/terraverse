// TypeScript mods, in a process of their own (sprint 119).
//
// `terra runtime <server folder>` loads every mod's server half from .terra/ and sends
// the content they declared. Then each server step opens a window: the step's events go
// over, mod code runs, and until the sidecar says it's idle the engine answers its calls.
// Mod code only runs inside a window, so what it reads of the world is synchronous and
// in order with what it wrote. A sidecar that dies is restarted; the server runs on.
#pragma once

#include "src/server/app/mods/sidecar/content.h"
#include "src/shared/base/irrlichttypes.h"
#include <array>
#include <chrono>
#include <functional>
#include <memory>
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
	// Every command mods have now, by first word: the whole set, replacing the last.
	struct Command { std::string name, description, params; };
	std::function<void(const std::vector<Command> &commands)> commands;
	// Whether a player holds a permission ("kilns:light").
	std::function<bool(const std::string &player, const std::string &permission)> can;
	std::function<void(const std::string &player, const std::string &reason)> kick;
};

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
