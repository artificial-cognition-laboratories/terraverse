// Terraverse: who may do what on this server (sprint 119). No passwords: everyone is their
// ArcLabs account. A world played alone admits its one player (AUTH_MECHANISM_LOCAL), a hosted
// one admits by ticket (src/online/gate).
//
// A permission is `prefix:name`: the engine's are `core:` (core:fly, core:interact), a mod's carry
// its prefix (commands:kick). A role is a set of them: `player` is everyone, `builder` builds in
// creative, `admin` may do anything, mods' permissions included. Roles are kept with the world
// (<world>/access.json); a grant is one permission for one player, until they leave.
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

class Access
{
public:
	// owner: the account whose world this is (the one playing alone, or hosting), always an
	// admin; "" for a server nobody owns from inside (headless: its console is its admin)
	Access(const std::string &world_path, const std::string &owner);

	// Every role's name, as the docs give them: player, builder, admin.
	static const std::vector<std::string> &roleNames();
	// The engine's own permissions, all an admin is told it has (the client checks these).
	static const std::vector<std::string> &corePermissions();

	bool can(const std::string &account, const std::string &permission) const;
	// What they hold: their roles' permissions and their grants. An admin's are every core: one.
	std::set<std::string> permissions(const std::string &account) const;
	// Their roles: `player` always, the owner `admin` always, and whatever's kept.
	std::vector<std::string> roles(const std::string &account) const;

	// Kept with the world. Throws for a role that doesn't exist.
	void addRole(const std::string &account, const std::string &role);
	void removeRole(const std::string &account, const std::string &role);

	void grant(const std::string &account, const std::string &permission);
	void revoke(const std::string &account, const std::string &permission);
	void left(const std::string &account) { m_grants.erase(account); }

private:
	void save() const;

	std::string m_path;
	std::string m_owner;
	std::map<std::string, std::set<std::string>> m_roles;  // kept: account -> roles beyond player
	std::map<std::string, std::set<std::string>> m_grants; // until they leave
};

// `prefix:name`, each a-z, 0-9, - or _: throws std::invalid_argument for anything else.
void checkPermissionName(const std::string &permission);
