// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "scene.h"
#include <json/json.h>
#include <string>
#include <vector>

namespace shell {

/*
	The wiki's Worlds time-lapse: a real patch of a real world through the
	erosion rounds that made it (far/landbake), as columns. It replays what
	terragen did; nothing is worked out here but how to colour it.
*/
class Relief
{
public:
	explicit Relief(const Json::Value &baked); // throws BaseException on what isn't one
	void update(float dt);
	Scene::Ground ground() const;
	// The patch's most of what a lens shows at the end: where to look (scene's frame).
	v3f focus() const;
	Json::Value state() const;
	void touch() { m_version++; }
	double totalSediment(int round) const;
	int rounds() const { return (int)m_land.size(); }
	int lowestOnRiver(int round) const;
	int richest() const;

	std::string lens = "land"; // land, worn, sediment, heavy, flow
	bool running = true;
	double speed = 1;
	double at = 0; // rounds

private:
	int m_side;
	std::vector<int> m_columns;
	std::vector<std::vector<int>> m_land, m_sediment, m_stream;
	std::vector<int> m_core, m_placer, m_lake, m_river;
	int m_highest = 1;
	u64 m_version = 0;
	float m_hold = 0;
};

} // namespace shell
