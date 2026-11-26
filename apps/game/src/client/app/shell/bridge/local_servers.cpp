// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/bridge/local_servers.h"
#include "src/client/app/shell/bridge/bridge.h"
#include "src/shared/platform/system/filesys.h"
#include "src/shared/platform/system/log.h"
#include "src/shared/util/text/string.h"

#include <optional>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#include <cerrno>
#endif

namespace shell {

namespace {

// One record, as terra writes it.
struct Record
{
	std::string ref, title, secret, server;
	std::vector<std::string> mods;
	std::optional<std::string> owner;
	uint16_t port;
	long pid;
	int64_t started;
};

std::vector<Record> running(const std::string &dir);
std::optional<Record> parse(const Json::Value &v);
bool alive(long pid);
bool loopback(const std::string &host);

} // namespace

Json::Value LocalServers::list() const
{
	Json::Value out(Json::arrayValue);
	for (const Record &r : running(m_opts.dir)) {
		Json::Value s(Json::objectValue);
		s["ref"] = r.ref;
		s["title"] = r.title;
		s["port"] = r.port;
		s["owner"] = r.owner ? Json::Value(*r.owner) : Json::Value();
		s["folder"] = r.server;
		s["started"] = (Json::Int64)r.started;
		s["mods"] = Json::Value(Json::arrayValue);
		for (const std::string &m : r.mods)
			s["mods"].append(m);
		out.append(s);
	}
	return out;
}

online::LocalJoin LocalServers::join(const online::ref::Address &address) const
{
	const std::string ref = online::ref::print(address);
	if (!loopback(address.host))
		throw Refused("Joining a server elsewhere (" + ref + ") isn't built yet: only servers on this machine.");
	for (const Record &r : running(m_opts.dir))
		if (r.port == address.port)
			return { address.host, address.port, r.secret, r.title };
	throw Refused("Nothing of terra's is running at " + ref + ". Start one with `terra serve`.");
}

namespace {

std::vector<Record> running(const std::string &dir)
{
	std::vector<Record> out;
	for (const fs::DirListNode &node : fs::GetDirListing(dir)) {
		if (node.dir || !str_ends_with(node.name, ".json"))
			continue;
		const std::string path = dir + DIR_DELIM + node.name;
		std::string text;
		if (!fs::ReadFile(path, text))
			continue; // gone as it was read: it stopped
		Json::Value v;
		std::string errors;
		Json::CharReaderBuilder builder;
		std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
		std::optional<Record> r;
		if (reader->parse(text.data(), text.data() + text.size(), &v, &errors))
			r = parse(v);
		// terra clears what isn't a record, or whose server stopped, when it next looks
		if (!r) {
			warningstream << "running/: " << node.name << " isn't a running server's record" << std::endl;
			continue;
		}
		if (alive(r->pid))
			out.push_back(std::move(*r));
	}
	return out;
}

// The seam: a record whole, or none.
std::optional<Record> parse(const Json::Value &v)
{
	if (!v.isObject())
		return std::nullopt;
	for (const char *key : { "ref", "title", "secret", "server" })
		if (!v[key].isString())
			return std::nullopt;
	if (!v["port"].isUInt() || v["port"].asUInt() < 1 || v["port"].asUInt() > 65535 ||
			!v["pid"].isIntegral() || !v["started"].isIntegral() || !v["mods"].isArray() ||
			!(v["owner"].isNull() || v["owner"].isString()))
		return std::nullopt;
	Record r{ v["ref"].asString(), v["title"].asString(), v["secret"].asString(), v["server"].asString(),
		{}, std::nullopt, (uint16_t)v["port"].asUInt(), (long)v["pid"].asInt64(), v["started"].asInt64() };
	if (v["owner"].isString())
		r.owner = v["owner"].asString();
	for (const Json::Value &m : v["mods"]) {
		if (!m.isString())
			return std::nullopt;
		r.mods.push_back(m.asString());
	}
	return r;
}

bool alive(long pid)
{
#ifdef _WIN32
	HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
	if (!h)
		return false;
	DWORD code = 0;
	bool running = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
	CloseHandle(h);
	return running;
#else
	return kill((pid_t)pid, 0) == 0 || errno == EPERM;
#endif
}

bool loopback(const std::string &host)
{
	return host == "localhost" || host == "::1" || host.rfind("127.", 0) == 0;
}

} // namespace

} // namespace shell
