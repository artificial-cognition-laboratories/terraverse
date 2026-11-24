// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>
#include <string>
#include <unordered_map>
#include <optional>
#include <vector>

class Client;
struct ItemDefinition;
namespace video { class IVideoDriver; }

namespace shell {

/*
	What the page needs to show an item: its label and an icon it can load.
	Icons are rendered by the engine (blocks are composed cubes, not files),
	written once per session into `dir`, and served as shell://item/<file>.
*/
class Items
{
public:
	struct Opts {
		Client *client;
		video::IVideoDriver *driver;
		std::string dir;
		std::string session; // cache-buster: icons can differ between sessions
	};

	explicit Items(const Opts &opts) : m_opts(opts) {}

	bool known(const std::string &name) const { return m_described.count(name) != 0; }
	// Main thread only: may render the icon.
	const Json::Value &describe(const std::string &name);
	// All visible registered items, with icons generated on the main thread.
	Json::Value catalog();
	// Main thread, each frame: describe the catalog's items ahead of being asked, for about
	// `budget_ms`, so opening the creative inventory renders no icons of its own.
	void warm(float budget_ms);

private:
	std::string iconTexture(const ItemDefinition &def) const;
	// A cube's faces flat, for the library's viewer: { top, bottom, side } URLs, or null for
	// what isn't drawn as a cube.
	Json::Value faces(const ItemDefinition &def);
	// A texture rendered to a file in `dir`: its shell://item/ URL.
	std::string served(const std::string &texture, const std::string &file) const;
	void writeIcon(const std::string &texture, const std::string &path) const;

	// The items the creative inventory lists, in order (computed once: the definitions are fixed in a session)
	const std::vector<std::string> &listed();

	Opts m_opts;
	std::unordered_map<std::string, Json::Value> m_described;
	std::optional<std::vector<std::string>> m_listed;
	size_t m_warmed = 0; // how far warm() has got through listed()
};

} // namespace shell
