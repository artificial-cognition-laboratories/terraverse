// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>
#include <string>

namespace home { class Home; }

namespace shell {

class Account;

/*
	The accounts this machine remembers: a profile whose saved key is for this backend
	(home.md, "Profiles"). Switching to one signs in with its key at once, no browser;
	signing out of one keeps its key for next time, unless it's forgotten.
*/
class Profiles
{
public:
	struct Opts {
		home::Home &home;
		Account &account;
		std::string api; // the ArcLabs backend (arclabs_url): a key is only good for its own
		std::string page; // the page this game shows (Shell::Opts::url), so another one shows it too
	};

	explicit Profiles(Opts opts) : m_opts(std::move(opts)) {}

	// The file a profile keeps its key for `api` in: one per backend, so a local stack's
	// key never overwrites production's.
	static std::string keyPath(const std::string &profile_dir, const std::string &api);

	// Those remembered, as the page shows them (bridge RememberedProfile[]).
	Json::Value list() const;
	// Run as `name` and sign in with its key. Throws Refused for one not remembered.
	void use(const std::string &name);
	// Signed out of this one, its key kept: back to playing signed out.
	void leave();
	// Signed out of this one, and its key forgotten.
	void signOut();
	// Forgets `name`'s key. Throws Refused for the one signed in now.
	void forget(const std::string &name);
	// Another game beside this one, as `name` (Play as): this executable again, on the same
	// backend and page. Throws Refused for one not remembered, or the one this game is.
	void playAs(const std::string &name) const;

private:
	std::string dirOf(const std::string &name) const;
	void toLocal();

	Opts m_opts;
};

} // namespace shell
