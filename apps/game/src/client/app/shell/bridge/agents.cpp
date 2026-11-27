// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/bridge/agents.h"
#include "src/client/app/shell/bridge/axond.h"
#include "src/client/app/shell/bridge/bridge.h"
#include "src/shared/platform/system/log.h"
#include <map>
#include <mutex>
#include <thread>

namespace shell {

namespace {

// Asking: an agent's boot is slow (embark waits for it); everything else answers in moments.
constexpr int EMBARK_MS = 120'000;
constexpr int ASK_MS = 10'000;
// Going: the world is leaving, and the player waits on it.
constexpr int GOING_MS = 3'000;

struct Here { std::string agent, name, world; }; // brought in: as asked for, as it's called, the lab's world address
struct Asked { std::string name; std::optional<std::string> failed; }; // on its way, or why it never came

std::string nameOf(const std::string &agent);
// What the list says of one agent: here, on its way, failed, running elsewhere, or idle.
Json::Value rowOf(const Agents::Shared &shared, const Json::Value &live, const std::string &agent, const std::string &name);
// A row of the list.
Json::Value row(const std::string &agent, const std::string &name, const char *state,
		const std::string *session, const std::string *reason);

} // namespace

struct Agents::Shared
{
	std::mutex lock;
	std::map<std::string, Here> here;   // by session
	std::map<std::string, Asked> asked; // by agent, as asked for
	bool gone = false;                  // the world's left: what comes in now is sent away
};

Agents::Agents(Opts opts) :
	m_shell(opts.shell),
	m_world(std::move(opts.world)),
	m_shared(std::make_shared<Shared>()),
	m_intents(opts.shell)
{
	m_intents.onAsync("agents.list", [this](const Json::Value &) { return list(); });
	m_intents.onAsync("agents.embark", [this](const Json::Value &intent) { return embark(intent); });
	m_intents.onAsync("agents.stop", [this](const Json::Value &intent) { return stop(intent); });
}

Agents::~Agents()
{
	m_intents.clear();
	m_shell.drain(); // no verb of ours is still running
	std::map<std::string, Here> here;
	{
		std::lock_guard<std::mutex> held(m_shared->lock);
		m_shared->gone = true;
		here.swap(m_shared->here);
	}
	if (here.empty())
		return;
	const Axond axond({ Axond::socketPath(), GOING_MS });
	try {
		for (const auto &[session, agent] : here)
			axond.call({ "agents", "stop" }, Json::Value(session));
		// the world, no longer spoken for: still the game's, gone in a moment
		const std::string world = here.begin()->second.world;
		Json::Value which(Json::objectValue);
		which["name"] = world.substr(world.rfind('/') + 1);
		axond.call({ "labs", "terraverse", "stop" }, which);
	} catch (const std::exception &e) {
		// can't throw from here: said, and axond's own sweep takes back what it can
		warningstream << "agents: couldn't send them all away as the world went: " << e.what() << std::endl;
	}
}

Json::Value Agents::list()
{
	Json::Value out(Json::objectValue);
	out["world"] = m_world.has_value();
	out["agents"] = Json::Value(Json::arrayValue);
	const Axond axond({ Axond::socketPath(), ASK_MS });
	Json::Value installed, live;
	try {
		installed = axond.call({ "agents", "own", "installed" }, Json::Value());
		live = axond.call({ "agents", "list" }, Json::Value());
	} catch (const Axond::Down &e) {
		out["daemon"] = "down";
		out["reason"] = e.what();
		return out;
	}
	out["daemon"] = "up";
	std::lock_guard<std::mutex> held(m_shared->lock);
	// what went on its own (stopped elsewhere, crashed) isn't here
	for (auto it = m_shared->here.begin(); it != m_shared->here.end();) {
		bool alive = false;
		for (const Json::Value &record : live)
			alive = alive || record["sessionId"].asString() == it->first;
		it = alive ? std::next(it) : m_shared->here.erase(it);
	}
	std::map<std::string, bool> listed;
	auto add = [&](const std::string &agent, const std::string &name) {
		if (listed[agent])
			return;
		listed[agent] = true;
		out["agents"].append(rowOf(*m_shared, live, agent, name));
	};
	// brought in by path or ref, beside what's installed: listed while they're ours
	for (const auto &[session, h] : m_shared->here)
		add(h.agent, h.name);
	for (const auto &[agent, a] : m_shared->asked)
		add(agent, a.name);
	for (const Json::Value &agent : installed)
		add(agent["root"].asString(), agent["ref"].asString());
	return out;
}

Json::Value Agents::embark(const Json::Value &intent)
{
	if (!intent["agent"].isString() || intent["agent"].asString().empty())
		throw Refused("missing string field 'agent'");
	if (!m_world)
		throw Refused("This world isn't open to agents: sign in, then open it again.");
	const std::string agent = intent["agent"].asString();
	{
		std::lock_guard<std::mutex> held(m_shared->lock);
		for (const auto &[session, h] : m_shared->here)
			if (h.agent == agent)
				throw Refused(h.name + " is already here.");
		auto asked = m_shared->asked.find(agent);
		if (asked != m_shared->asked.end() && !asked->second.failed)
			throw Refused(asked->second.name + " is on its way.");
		m_shared->asked[agent] = { nameOf(agent), std::nullopt };
	}
	// its own thread: an agent boots in seconds, and the bridge's worker is everyone's
	std::thread([shared = m_shared, agent, world = *m_world] {
		const Axond axond({ Axond::socketPath(), EMBARK_MS });
		Json::Value ask(Json::objectValue);
		ask["agent"] = agent;
		ask["world"] = world;
		try {
			const Json::Value came = axond.call({ "embark" }, ask);
			const std::string session = came["session"].asString();
			std::unique_lock<std::mutex> held(shared->lock);
			if (shared->gone) {
				held.unlock();
				axond.call({ "agents", "stop" }, Json::Value(session)); // the world left while it booted
				return;
			}
			shared->asked.erase(agent);
			shared->here[session] = { agent, came["name"].asString(), came["world"].asString() };
		} catch (const std::exception &e) {
			std::lock_guard<std::mutex> held(shared->lock);
			shared->asked[agent].failed = e.what();
		}
	}).detach();
	return Json::Value(Json::objectValue);
}

Json::Value Agents::stop(const Json::Value &intent)
{
	if (!intent["session"].isString())
		throw Refused("missing string field 'session'");
	const std::string session = intent["session"].asString();
	Axond({ Axond::socketPath(), ASK_MS }).call({ "agents", "stop" }, Json::Value(session));
	std::lock_guard<std::mutex> held(m_shared->lock);
	m_shared->here.erase(session);
	return Json::Value(Json::objectValue);
}

namespace {

Json::Value rowOf(const Agents::Shared &shared, const Json::Value &live, const std::string &agent, const std::string &name)
{
	for (const auto &[session, h] : shared.here)
		if (h.agent == agent)
			return row(agent, name, "here", &session, nullptr);
	auto asked = shared.asked.find(agent);
	if (asked != shared.asked.end())
		return asked->second.failed
				? row(agent, name, "failed", nullptr, &*asked->second.failed)
				: row(agent, name, "starting", nullptr, nullptr);
	for (const Json::Value &record : live)
		if (record["projectRoot"].asString() == agent || record["agentName"].asString() == agent)
			return row(agent, name, "elsewhere", nullptr, nullptr);
	return row(agent, name, "idle", nullptr, nullptr);
}

std::string nameOf(const std::string &agent)
{
	// a path's last folder (…/agents/terry); a ref as it is (@cody/terry)
	if (agent.empty() || agent[0] == '@')
		return agent;
	const size_t slash = agent.find_last_of('/', agent.size() - 2);
	std::string name = slash == std::string::npos ? agent : agent.substr(slash + 1);
	if (!name.empty() && name.back() == '/')
		name.pop_back();
	return name;
}

Json::Value row(const std::string &agent, const std::string &name, const char *state,
		const std::string *session, const std::string *reason)
{
	Json::Value r(Json::objectValue);
	r["agent"] = agent;
	r["name"] = name;
	r["state"] = state;
	r["session"] = session ? Json::Value(*session) : Json::Value();
	r["reason"] = reason ? Json::Value(*reason) : Json::Value();
	return r;
}

} // namespace

} // namespace shell
