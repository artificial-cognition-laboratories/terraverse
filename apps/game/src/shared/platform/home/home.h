// Terraverse: where Terraverse keeps things on this machine (.agents/planning/home.md): one
// root, a profile per player, a cache that's safe to delete. The engine's legacy globals
// (porting::path_user, path_cache) are set from here and follow the active profile.
#pragma once

#include <memory>
#include <stdexcept>
#include <string>

namespace home {

struct Error : std::runtime_error {
	using std::runtime_error::runtime_error;
};

class Home
{
public:
	struct Opts {
		std::string root;   // absolute; empty for TERRAVERSE_HOME, else the build's own (~/.terraverse[-dev])
		std::string chosen; // the profile asked for (--profile, TERRAVERSE_PROFILE); empty for the active one
		std::string legacy; // where an older build kept its data (a source build's .output): only moved into the build's own home
	};

	// The root made ready: what an older layout left moved in (once), the profile picked.
	// Throws Error saying what couldn't be made or read.
	static std::unique_ptr<Home> boot(Opts o);

	// This build's root when nothing names one: installed builds and source builds never share.
	static std::string defaultRoot();

	const std::string &root() const { return m_root; }
	std::string cache() const;
	// The engine's own rebuildable files: media, grids, far tiles.
	std::string engineCache() const;
	std::string logs() const;
	std::string settingsFile() const;
	// The profile running now ("local", "@cody", "two"), and its folder.
	const std::string &profile() const { return m_profile; }
	std::string profileDir() const;
	// Chromium's cache for this process: fixed by the profile it started as.
	std::string browserCache() const { return m_browser; }

	// Run as `name` from now on (created if it's new) and remember it as the active one.
	// Throws Error for a name a profile can't have.
	void use(const std::string &name);
	// An ArcLabs account signed in: its profile, adopting `local` when it has none yet.
	// The account's key file, moved into it, is returned at its new path.
	std::string signedIn(const std::string &handle, const std::string &key_file);
	// Signed out: back to `local`, the account's profile kept for next time.
	void signedOut();

	// Whether a name is one a profile can have: local, @handle, or a plain test player's.
	static bool validName(const std::string &name);

private:
	Home(std::string root, std::string profile, std::string browser);
	void point();

	const std::string m_root;
	std::string m_profile;
	const std::string m_browser;
};

} // namespace home
