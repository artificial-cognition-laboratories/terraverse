// Terraverse: who may do what on this server (sprint 119). See access.h.
#include "src/server/app/access/access.h"

#include "src/shared/base/exceptions.h"
#include "src/shared/platform/system/filesys.h"
#include <algorithm>
#include <fstream>
#include <json/json.h>
#include <stdexcept>

namespace {

const char *ADMIN = "admin";
const char *PLAYER = "player";

// What each role holds beyond the one before it; admin holds everything, so it isn't listed.
const std::map<std::string, std::vector<std::string>> &rolePermissions()
{
	static const std::map<std::string, std::vector<std::string>> roles = {
		{ PLAYER, { "core:interact", "core:shout" } },
		{ "builder", { "core:interact", "core:shout", "core:creative", "core:fly", "core:fast",
				"core:noclip", "core:give" } },
	};
	return roles;
}

bool word(const std::string &s)
{
	return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) {
		return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
	});
}

} // namespace

void checkPermissionName(const std::string &permission)
{
	const size_t colon = permission.find(':');
	if (colon == std::string::npos || !word(permission.substr(0, colon)) ||
			!word(permission.substr(colon + 1)))
		throw std::invalid_argument("a permission is prefix:name (a-z, 0-9, - and _), not \"" +
				permission + "\"");
}

const std::vector<std::string> &Access::roleNames()
{
	static const std::vector<std::string> names = { PLAYER, "builder", ADMIN };
	return names;
}

const std::vector<std::string> &Access::corePermissions()
{
	static const std::vector<std::string> all = { "core:interact", "core:shout", "core:creative",
			"core:fly", "core:fast", "core:noclip", "core:give", "core:debug", "core:server" };
	return all;
}

Access::Access(const std::string &world_path, const std::string &owner) :
	m_path(world_path + DIR_DELIM "access.json"), m_owner(owner)
{
	if (!fs::PathExists(m_path))
		return; // nobody's been given a role yet
	std::ifstream in(m_path);
	Json::Value root;
	Json::CharReaderBuilder reader;
	std::string errors;
	if (!Json::parseFromStream(reader, in, &root, &errors))
		throw BaseException("access: " + m_path + " isn't JSON: " + errors);
	const Json::Value &roles = root["roles"];
	if (!root.isObject() || !roles.isObject())
		throw BaseException("access: " + m_path + " is { \"roles\": { account: [role…] } }");
	for (const std::string &account : roles.getMemberNames()) {
		const Json::Value &list = roles[account];
		if (!list.isArray())
			throw BaseException("access: " + m_path + ": " + account + "'s roles aren't a list");
		for (const Json::Value &role : list) {
			const std::string name = role.isString() ? role.asString() : "";
			if (std::find(roleNames().begin(), roleNames().end(), name) == roleNames().end() ||
					name == PLAYER)
				throw BaseException("access: " + m_path + ": " + account + " has no role \"" +
						name + "\" to have (builder or admin; everyone is a player)");
			m_roles[account].insert(name);
		}
	}
}

std::vector<std::string> Access::roles(const std::string &account) const
{
	std::vector<std::string> out = { PLAYER };
	auto kept = m_roles.find(account);
	for (const std::string &role : roleNames()) {
		if (role == PLAYER)
			continue;
		const bool has = (kept != m_roles.end() && kept->second.count(role)) ||
				(role == ADMIN && !m_owner.empty() && account == m_owner);
		if (has)
			out.push_back(role);
	}
	return out;
}

std::set<std::string> Access::permissions(const std::string &account) const
{
	std::set<std::string> out;
	for (const std::string &role : roles(account)) {
		if (role == ADMIN) {
			out.insert(corePermissions().begin(), corePermissions().end());
			continue;
		}
		const std::vector<std::string> &held = rolePermissions().at(role);
		out.insert(held.begin(), held.end());
	}
	auto granted = m_grants.find(account);
	if (granted != m_grants.end())
		out.insert(granted->second.begin(), granted->second.end());
	return out;
}

bool Access::can(const std::string &account, const std::string &permission) const
{
	const std::vector<std::string> held = roles(account);
	if (std::find(held.begin(), held.end(), ADMIN) != held.end())
		return true; // everything, a mod's permissions too
	return permissions(account).count(permission) != 0;
}

void Access::addRole(const std::string &account, const std::string &role)
{
	if (role == PLAYER)
		return; // everyone is one
	if (std::find(roleNames().begin(), roleNames().end(), role) == roleNames().end())
		throw std::invalid_argument("there's no role \"" + role + "\": builder or admin");
	if (m_roles[account].insert(role).second)
		save();
}

void Access::removeRole(const std::string &account, const std::string &role)
{
	if (role == PLAYER)
		throw std::invalid_argument("everyone is a player");
	if (role == ADMIN && account == m_owner)
		throw std::invalid_argument(account + " owns this world: they're always its admin");
	auto kept = m_roles.find(account);
	if (kept == m_roles.end() || !kept->second.erase(role))
		return;
	if (kept->second.empty())
		m_roles.erase(kept);
	save();
}

void Access::grant(const std::string &account, const std::string &permission)
{
	checkPermissionName(permission);
	m_grants[account].insert(permission);
}

void Access::revoke(const std::string &account, const std::string &permission)
{
	auto granted = m_grants.find(account);
	if (granted == m_grants.end())
		return;
	granted->second.erase(permission);
	if (granted->second.empty())
		m_grants.erase(granted);
}

void Access::save() const
{
	Json::Value roles(Json::objectValue);
	for (const auto &[account, held] : m_roles) {
		Json::Value list(Json::arrayValue);
		for (const std::string &role : held)
			list.append(role);
		roles[account] = list;
	}
	Json::Value root(Json::objectValue);
	root["roles"] = roles;
	Json::StreamWriterBuilder writer;
	writer["indentation"] = "\t";
	if (!fs::safeWriteToFile(m_path, Json::writeString(writer, root) + "\n"))
		throw BaseException("access: couldn't write " + m_path);
}
