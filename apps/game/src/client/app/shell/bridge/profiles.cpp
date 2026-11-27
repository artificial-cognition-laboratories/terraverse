// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/bridge/profiles.h"
#include "src/client/app/shell/bridge/account.h"
#include "src/client/app/shell/bridge/bridge.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/platform/home/home.h"
#include "src/shared/platform/settings/settings.h"
#include "src/shared/platform/system/filesys.h"
#include "src/shared/platform/system/porting.h"
#include "src/shared/platform/system/spawn.h"

#include <cctype>

namespace shell {

std::string Profiles::keyPath(const std::string &dir, const std::string &api)
{
	if (!fs::CreateAllDirs(dir))
		throw BaseException("shell: could not create " + dir);
	if (api == Settings::getLayer(SL_DEFAULTS)->get("arclabs_url"))
		return dir + DIR_DELIM "account.json";
	std::string host = api.substr(api.find("://") == std::string::npos ? 0 : api.find("://") + 3);
	for (char &c : host) {
		if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '-')
			c = '_';
	}
	return dir + DIR_DELIM "account-" + host + ".json";
}

Json::Value Profiles::list() const
{
	Json::Value out(Json::arrayValue);
	for (const fs::DirListNode &node : fs::GetDirListing(m_opts.home.root() + DIR_DELIM "profiles")) {
		// an account's profile is @handle; others (local, a test player's) have no key to remember
		if (!node.dir || node.name.empty() || node.name[0] != '@' || !home::Home::validName(node.name))
			continue;
		if (!fs::PathExists(keyPath(dirOf(node.name), m_opts.api)))
			continue;
		Json::Value p(Json::objectValue);
		p["name"] = node.name;
		p["handle"] = node.name.substr(1);
		p["current"] = node.name == m_opts.home.profile();
		out.append(p);
	}
	return out;
}

void Profiles::use(const std::string &name)
{
	if (!home::Home::validName(name) || name[0] != '@' || !fs::PathExists(keyPath(dirOf(name), m_opts.api)))
		throw Refused("This machine doesn't remember " + name + ": sign in to add it.");
	if (!m_opts.account.signedOut())
		m_opts.account.leave();
	m_opts.home.use(name);
	m_opts.account.keyAt(keyPath(m_opts.home.profileDir(), m_opts.api));
	m_opts.account.resume();
}

void Profiles::leave()
{
	m_opts.account.leave();
	toLocal();
}

void Profiles::signOut()
{
	m_opts.account.signOut();
	toLocal();
}

void Profiles::forget(const std::string &name)
{
	if (!home::Home::validName(name) || name[0] != '@')
		throw Refused("No profile is called " + name + ".");
	if (name == m_opts.home.profile() && !m_opts.account.signedOut())
		throw Refused("Sign out of " + name + " first.");
	const std::string key = keyPath(dirOf(name), m_opts.api);
	if (fs::PathExists(key) && !fs::DeleteSingleFileOrEmptyDirectory(key, true))
		throw BaseException("shell: could not delete " + key);
}

void Profiles::playAs(const std::string &name) const
{
	if (!home::Home::validName(name) || name[0] != '@' || !fs::PathExists(keyPath(dirOf(name), m_opts.api)))
		throw Refused("This machine doesn't remember " + name + ": sign in to add it.");
	if (name == m_opts.home.profile())
		throw Refused("This game is " + name + " already.");
	char exe[4096];
	if (!porting::getCurrentExecPath(exe, sizeof(exe)))
		throw BaseException("shell: can't find this game's own executable to start another");
	// TERRAVERSE_HOME and the rest come with the environment; --profile outranks TERRAVERSE_PROFILE
	if (!spawn::detached({ exe, "--profile", name, "--shell-url", m_opts.page, "--arclabs-url", m_opts.api }))
		throw Refused("Another game couldn't be started.");
}

std::string Profiles::dirOf(const std::string &name) const
{
	return m_opts.home.root() + DIR_DELIM "profiles" DIR_DELIM + name;
}

// back to playing signed out; the account's profile is kept for its next sign-in
void Profiles::toLocal()
{
	m_opts.home.signedOut();
	m_opts.account.keyAt(keyPath(m_opts.home.profileDir(), m_opts.api));
}

} // namespace shell
