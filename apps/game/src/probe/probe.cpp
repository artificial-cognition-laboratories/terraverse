// Terra Firma: the checks' probes (sprint 119). See probe.h.
#include "probe.h"

#include "emerge.h"
#include "exceptions.h"
#include "itemgroup.h"
#include "log.h"
#include "map.h"
#include "nodedef.h"
#include "server.h"
#include "server/terra.h"
#include "serverenvironment.h"
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <map>

namespace probe {

// The probes, each in its own file.
void mods(Context &c);
void matter(Context &c);
void atlasDig(Context &c);
void atlasDebug(Context &c);
void atlasWorld(Context &c);
void atlasLand(Context &c);
void atlasSeams(Context &c);
void window(Context &c);
void bodies(Context &c);
void far(Context &c);
void terragen(Context &c);
void interactProbe(Context &c);

namespace {

const std::map<std::string, void (*)(Context &)> &probes()
{
	static const std::map<std::string, void (*)(Context &)> all = {
		{ "mods", mods }, { "matter", matter }, { "atlas-dig", atlasDig }, { "atlas-debug", atlasDebug },
		{ "atlas-world", atlasWorld }, { "atlas-land", atlasLand }, { "atlas-seams", atlasSeams },
		{ "window", window }, { "bodies", bodies }, { "far", far }, { "terragen", terragen },
		{ "interact", interactProbe },
	};
	return all;
}

// One map block made: its request's count goes up (on the emerge thread; the server's
// thread reads it each step).
void emerged(v3s16, EmergeAction, void *param)
{
	auto *done = static_cast<std::shared_ptr<std::atomic<size_t>> *>(param);
	(**done)++;
	delete done;
}

} // namespace

std::string format(const char *fmt, ...)
{
	char buf[4096];
	va_list args;
	va_start(args, fmt);
	std::vsnprintf(buf, sizeof buf, fmt, args);
	va_end(args);
	return buf;
}

const char *regionName(terragen::Atlas::Region region)
{
	static const char *const NAMES[] = { "segment", "cap", "core" };
	return NAMES[(int)region];
}

Context::~Context() = default;

ServerEnvironment &Context::env()
{
	return m_server->getEnv();
}

void Context::after(float seconds, std::function<void()> fn)
{
	m_timers.push_back({ m_now + seconds, std::move(fn) });
}

void Context::emerge(v3s16 minp, v3s16 maxp, std::function<void()> fn)
{
	const v3s16 a = getNodeBlockPos(v3s16(std::min(minp.X, maxp.X), std::min(minp.Y, maxp.Y), std::min(minp.Z, maxp.Z)));
	const v3s16 b = getNodeBlockPos(v3s16(std::max(minp.X, maxp.X), std::max(minp.Y, maxp.Y), std::max(minp.Z, maxp.Z)));
	auto done = std::make_shared<std::atomic<size_t>>(0);
	size_t blocks = 0;
	EmergeManager *emerge = m_server->getEmergeManager();
	for (s16 z = a.Z; z <= b.Z; z++)
		for (s16 y = a.Y; y <= b.Y; y++)
			for (s16 x = a.X; x <= b.X; x++) {
				blocks++;
				emerge->enqueueBlockEmergeEx(v3s16(x, y, z), PEER_ID_INEXISTENT,
						BLOCK_EMERGE_ALLOW_GEN | BLOCK_EMERGE_FORCE_QUEUE, emerged,
						new std::shared_ptr<std::atomic<size_t>>(done));
			}
	m_emerging.push_back({ blocks, done, std::move(fn) });
}

void Context::eachStep(std::function<void(float)> fn)
{
	m_each.push_back(std::move(fn));
}

void Context::say(const std::string &line)
{
	std::cout << line << std::endl;
}

void Context::finish()
{
	if (m_finished)
		return;
	m_finished = true;
	m_server->requestShutdown("probe done", false, 0);
}

std::string Context::node(v3s16 p)
{
	return m_server->ndef()->get(env().getMap().getNode(p)).name;
}

void Context::set(v3s16 p, const std::string &name, u8 param2)
{
	content_t c;
	if (!m_server->ndef()->getId(name, c))
		throw BaseException("probe: no block called " + name);
	env().setNode(p, MapNode(c, 0, param2));
}

const ContentFeatures &Context::def(const std::string &name)
{
	return m_server->ndef()->get(name);
}

int Context::group(const std::string &name, const std::string &group)
{
	return itemgroup_get(def(name).groups, group);
}

void Context::keep(v3s16 p)
{
	env().getForceloadedBlocks()->insert(getNodeBlockPos(p));
}

const planet::Atlas &Context::atlas()
{
	if (const planet::Atlas *a = m_server->atlas())
		return *a;
	throw BaseException("probe: this world isn't an atlas planet");
}

std::optional<v3s16> Context::atlasNode(double lat, double lon, s64 layer, int body)
{
	const terragen::Atlas &a = atlas().body(body);
	if (layer >= a.params().height)
		return std::nullopt;
	const double r = a.params().radius + layer + 0.5;
	if (r <= 0)
		return std::nullopt; // past the centre
	const planet::Atlas::Cell c(body, a.locate({ r * std::cos(lat) * std::cos(lon), r * std::sin(lat),
			r * std::cos(lat) * std::sin(lon) }));
	const planet::Atlas::Node n = m_server->planetNode(c); // its tile put in the window
	return v3s16(n.x, n.y, n.z);
}

std::optional<planet::Atlas::Cell> Context::cell(v3s16 p)
{
	return atlas().cell({ p.X, p.Y, p.Z });
}

std::optional<std::pair<double, double>> Context::place(v3s16 p)
{
	const auto at = atlas().surface({ p.X, p.Y, p.Z });
	if (!at)
		return std::nullopt;
	return std::pair{ std::atan2(at->y, std::hypot(at->x, at->z)), std::atan2(at->z, at->x) };
}

v3s16 Context::spawn(int body)
{
	const std::optional<v3s16> n = terraLanding(m_server, body, std::nullopt);
	if (!n)
		throw BaseException("probe: no land within 60° of the equator to stand on");
	return *n;
}

int Context::body(const std::string &id)
{
	const std::vector<terragen::BodyWorld> &worlds = m_server->bodyWorlds();
	for (size_t b = 0; b < worlds.size(); b++)
		if (worlds[b].id == id)
			return (int)b;
	throw BaseException("probe: this world has no body " + id);
}

void Context::step(float dtime)
{
	m_now += dtime;
	for (const auto &fn : std::vector(m_each))
		fn(dtime);
	// (what's run may add more: taken out first)
	for (size_t i = 0; i < m_emerging.size();) {
		if (*m_emerging[i].done < m_emerging[i].blocks) {
			i++;
			continue;
		}
		std::function<void()> fn = std::move(m_emerging[i].fn);
		m_emerging.erase(m_emerging.begin() + i);
		fn();
	}
	for (size_t i = 0; i < m_timers.size();) {
		if (m_timers[i].at > m_now) {
			i++;
			continue;
		}
		std::function<void()> fn = std::move(m_timers[i].fn);
		m_timers.erase(m_timers.begin() + i);
		fn();
	}
}

std::unique_ptr<Context> start(const std::string &name, Server *server)
{
	auto it = probes().find(name);
	if (it == probes().end()) {
		std::string all;
		for (const auto &[n, _] : probes())
			all += (all.empty() ? "" : ", ") + n;
		throw BaseException("--probe " + name + ": there's no such probe (" + all + ")");
	}
	auto c = std::make_unique<Context>(server);
	it->second(*c);
	return c;
}

} // namespace probe
