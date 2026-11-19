// Terra Firma: the checks' probes (sprint 119: were Lua mods in scripts/*-check/probe).
// A check starts a headless server with --probe <name>; the probe drives the world as the
// check needs (makes areas, digs, pours, waits), prints lines the check's script reads
// (MATTER …, ATLAS …), and stops the server. Native, so a probe reaches what's inside the
// engine (the atlas's cells, the world model, the matter) that no mod's API offers.
//
// A probe is a function that sets up what's to happen (Context::after, emerge); it runs on
// the server's thread, one step at a time, never blocking it.
#pragma once

#include "irrlichttypes_bloated.h"
#include "mapnode.h"
#include "planet/atlas.h"
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct ContentFeatures;
class Server;
class ServerEnvironment;

namespace probe {

class Context
{
public:
	explicit Context(Server *server) : m_server(server) {}
	~Context();

	Server *server() { return m_server; }
	ServerEnvironment &env();

	// `fn` in that many seconds of the server's time
	void after(float seconds, std::function<void()> fn);
	// every map block of the area made (or loaded), then `fn`
	void emerge(v3s16 minp, v3s16 maxp, std::function<void()> fn);
	// `fn` each server step, with its dtime
	void eachStep(std::function<void(float)> fn);
	// a line for the check's script, on the server's stdout
	void say(const std::string &line);
	// done: the server stops
	void finish();

	// The world: a node's block's name, set by name, a block's definition.
	std::string node(v3s16 p);
	void set(v3s16 p, const std::string &name, u8 param2 = 0);
	const ContentFeatures &def(const std::string &name);
	int group(const std::string &name, const std::string &group);
	// a map block kept active with no one near (a headless server has no players)
	void keep(v3s16 p);

	// The planet. Each throws BaseException where the world isn't an atlas planet.
	const planet::Atlas &atlas();
	// The node of the cell at a place on a body (radians), that layer up; its tile put in
	// the map's window. None past the top.
	std::optional<v3s16> atlasNode(double lat, double lon, s64 layer, int body = planet::Atlas::HOME);
	std::optional<planet::Atlas::Cell> cell(v3s16 p);
	// latitude, longitude (radians) of a node's column; none on seam
	std::optional<std::pair<double, double>> place(v3s16 p);
	// where one lands on a body
	v3s16 spawn(int body = planet::Atlas::HOME);
	// a body's number by its id
	int body(const std::string &id);

	// Called by the server each step.
	void step(float dtime);

private:
	struct Timer { double at; std::function<void()> fn; };
	struct Emerging
	{
		size_t blocks;
		std::shared_ptr<std::atomic<size_t>> done;
		std::function<void()> fn;
	};
	Server *m_server;
	double m_now = 0;
	std::vector<Timer> m_timers;
	std::vector<Emerging> m_emerging;
	std::vector<std::function<void(float)>> m_each;
	bool m_finished = false;
};

// Starts the probe of that name on the server (after the world's opened). Throws
// BaseException naming the probes there are, for one that isn't.
std::unique_ptr<Context> start(const std::string &name, Server *server);

// printf for say()
std::string format(const char *fmt, ...);
// "segment", "cap" or "core"
const char *regionName(terragen::Atlas::Region region);

} // namespace probe
