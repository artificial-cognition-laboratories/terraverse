#include "src/shared/platform/home/home.h"

#include "src/shared/platform/home/browser.h"
#include "src/shared/platform/home/moving.h"
#include "src/shared/base/config.h"
#include "src/shared/platform/system/filesys.h"
#include "src/shared/platform/system/porting.h"
#include <json/json.h>
#include <cstdlib>
#include <ctime>
#include <sstream>

namespace home {

namespace {

const char *const LOCAL = "local";

std::string activeFile(const std::string &root);
std::string readActive(const std::string &root);
void writeJson(const std::string &path, const Json::Value &value);
void makeProfile(const std::string &dir, const std::string &name);
void move(const std::string &from, const std::string &to);

} // namespace

std::unique_ptr<Home> Home::boot(Opts o)
{
	if (o.root.empty()) {
		if (const char *env = getenv("TERRAVERSE_HOME"); env && *env) {
			o.root = env;
			// a home named on purpose (a test's, the launcher's) takes nothing from a build's folder
			o.legacy.clear();
		} else {
			o.root = defaultRoot();
		}
	}
	if (!fs::IsPathAbsolute(o.root))
		throw Error("the home folder must be an absolute path, not \"" + o.root + "\"");
	if (!fs::CreateAllDirs(o.root + DIR_DELIM "cache"))
		throw Error("could not create " + o.root);
	// once per layout: moving in is safe to repeat (what's moved is gone), so a cleared cache only re-checks
	if (!fs::PathExists(o.root + DIR_DELIM "cache" DIR_DELIM "home.json")) {
		moving::in(o.root, o.legacy);
		Json::Value marker;
		marker["layout"] = 1;
		writeJson(o.root + DIR_DELIM "cache" DIR_DELIM "home.json", marker);
	}
	if (!o.chosen.empty() && !validName(o.chosen))
		throw Error("--profile takes local, @handle, or letters, digits, _ and -: not \"" + o.chosen + "\"");
	const std::string profile = !o.chosen.empty() ? o.chosen : readActive(o.root);
	// a process asked for a profile (a second player beside the first) has a Chromium of its own,
	// and so does a second game of the same one (browser.h)
	const std::string browser = freeBrowserCache(o.root + DIR_DELIM "cache" DIR_DELIM "shell-" + (o.chosen.empty() ? "main" : o.chosen));
	std::unique_ptr<Home> home(new Home(o.root, profile, browser));
	home->point();
	// what this engine starts (terra: its sidecar, a mod's contents) keeps things where it does,
	// whichever build of terra it is (a release terra beside a source engine reads this home)
#ifdef _WIN32
	_putenv_s("TERRAVERSE_HOME", o.root.c_str());
#else
	setenv("TERRAVERSE_HOME", o.root.c_str(), 1);
#endif
	makeProfile(home->profileDir(), profile);
	return home;
}

std::string Home::defaultRoot()
{
	// installed and source builds never share a home: work on one never repoints the other
#ifdef _WIN32
	const char *appdata = getenv("APPDATA");
	if (!appdata || !*appdata)
		throw Error("APPDATA isn't set: there's nowhere to keep things");
	return std::string(appdata) + (TERRAVERSE_RELEASE ? DIR_DELIM "Terraverse" : DIR_DELIM "Terraverse-dev");
#else
	const char *user = getenv("HOME");
	if (!user || !*user)
		throw Error("HOME isn't set: there's nowhere to keep things");
	return std::string(user) + (TERRAVERSE_RELEASE ? DIR_DELIM ".terraverse" : DIR_DELIM ".terraverse-dev");
#endif
}

Home::Home(std::string root, std::string profile, std::string browser) :
	m_root(std::move(root)), m_profile(std::move(profile)), m_browser(std::move(browser))
{}

std::string Home::cache() const { return m_root + DIR_DELIM "cache"; }
std::string Home::engineCache() const { return cache() + DIR_DELIM "engine"; }
std::string Home::logs() const { return cache() + DIR_DELIM "logs"; }
std::string Home::settingsFile() const { return m_root + DIR_DELIM "settings.conf"; }
std::string Home::profileDir() const { return m_root + DIR_DELIM "profiles" DIR_DELIM + m_profile; }

void Home::use(const std::string &name)
{
	if (!validName(name))
		throw Error("not a profile's name: \"" + name + "\"");
	m_profile = name;
	makeProfile(profileDir(), name);
	Json::Value active;
	active["profile"] = name;
	writeJson(activeFile(m_root), active);
	point();
}

std::string Home::signedIn(const std::string &handle, const std::string &key_file)
{
	const std::string name = "@" + handle;
	if (!validName(name))
		throw Error("ArcLabs named the account \"" + handle + "\", which can't name a profile");
	const std::string dir = m_root + DIR_DELIM "profiles" DIR_DELIM + name;
	const std::string file = fs::GetFilenameFromPath(key_file.c_str());
	if (m_profile == name)
		return key_file;
	// the first sign-in here: what was played signed out becomes the account's
	if (m_profile == LOCAL && !fs::PathExists(dir)) {
		move(profileDir(), dir);
		Json::Value p;
		p["name"] = name;
		p["handle"] = handle;
		p["created"] = (Json::Int64)time(nullptr);
		writeJson(dir + DIR_DELIM "profile.json", p);
		use(name);
		return profileDir() + DIR_DELIM + file;
	}
	const std::string from = key_file;
	use(name);
	const std::string to = profileDir() + DIR_DELIM + file;
	move(from, to);
	return to;
}

void Home::signedOut()
{
	use(LOCAL);
}

bool Home::validName(const std::string &name)
{
	const std::string plain = !name.empty() && name[0] == '@' ? name.substr(1) : name;
	if (plain.empty() || plain.size() > 64)
		return false;
	for (char c : plain)
		if (!(isalnum((unsigned char)c) || c == '_' || c == '-' || (c == '.' && name[0] == '@')))
			return false;
	return plain != "." && plain != "..";
}

// The legacy globals follow the profile: everything that reads them reads it when it needs it.
void Home::point()
{
	porting::path_user = profileDir();
	porting::path_cache = engineCache();
	if (!fs::CreateAllDirs(porting::path_user) || !fs::CreateAllDirs(porting::path_cache))
		throw Error("could not create " + porting::path_user);
}

namespace {

std::string activeFile(const std::string &root)
{
	return root + DIR_DELIM "cache" DIR_DELIM "active-profile.json";
}

std::string readActive(const std::string &root)
{
	if (const char *env = getenv("TERRAVERSE_PROFILE"); env && *env) {
		if (!Home::validName(env))
			throw Error(std::string("TERRAVERSE_PROFILE isn't a profile's name: \"") + env + "\"");
		return env;
	}
	std::string text;
	if (!fs::ReadFile(activeFile(root), text))
		return LOCAL;
	Json::Value v;
	std::istringstream in(text);
	Json::CharReaderBuilder reader;
	std::string errors;
	if (!Json::parseFromStream(reader, in, &v, &errors) || !v["profile"].isString() || !Home::validName(v["profile"].asString()))
		throw Error(activeFile(root) + " isn't { \"profile\": <name> }: delete it to start as local");
	return v["profile"].asString();
}

void writeJson(const std::string &path, const Json::Value &value)
{
	Json::StreamWriterBuilder w;
	w["indentation"] = "  ";
	if (!fs::safeWriteToFile(path, Json::writeString(w, value) + "\n"))
		throw Error("could not write " + path);
}

void makeProfile(const std::string &dir, const std::string &name)
{
	if (!fs::CreateAllDirs(dir))
		throw Error("could not create " + dir);
	const std::string file = dir + DIR_DELIM "profile.json";
	if (fs::PathExists(file))
		return;
	Json::Value p;
	p["name"] = name;
	p["handle"] = name[0] == '@' ? Json::Value(name.substr(1)) : Json::Value();
	p["created"] = (Json::Int64)time(nullptr);
	writeJson(file, p);
}

void move(const std::string &from, const std::string &to)
{
	if (!fs::PathExists(from))
		return;
	if (fs::Rename(from, to))
		return;
	if (!fs::MoveDir(from, to))
		throw Error("could not move " + from + " to " + to);
}

} // namespace

} // namespace home
