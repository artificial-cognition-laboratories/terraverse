// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>
#include <string>
#include <unordered_map>

class Client;
struct ItemDefinition;
namespace video { class IVideoDriver; }

namespace shell {

/*
	What the page needs to show an item: its label and an icon it can load.
	Icons are rendered by the engine (blocks are composed cubes, not files),
	written once per session into `dir`, and served as terra://item/<file>.
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

private:
	std::string iconTexture(const ItemDefinition &def) const;
	void writeIcon(const std::string &texture, const std::string &path) const;

	Opts m_opts;
	std::unordered_map<std::string, Json::Value> m_described;
};

} // namespace shell
