// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>
#include <string>
#include <vector>

namespace shell {

class Worlds;

// Local content behind the Community screen. Discovery is a separate future
// source; this only reports and changes what this installation actually has.
class Community
{
public:
	Community(const Worlds &worlds, std::string gameid) :
		m_worlds(worlds), m_gameid(std::move(gameid)) {}

	Json::Value list() const;
	void setTexture(const std::string &id) const; // empty id restores default textures

private:
	const Worlds &m_worlds;
	std::string m_gameid;
};

} // namespace shell
