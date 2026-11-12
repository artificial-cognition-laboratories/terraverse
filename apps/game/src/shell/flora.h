// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "scene.h"
#include "flora/flora.h"
#include <json/json.h>
#include <map>
#include <vector>

namespace shell {

/*
	The wiki's Flora page: a plant grown by the game's own generator (flora/)
	from the site the page sets, one of a world's lineages at a time. Nothing
	is worked out here but how to draw it.
*/
class Flora
{
public:
	Flora();
	// { world: string, lineage: index, plant: its own seed, life: { age, season } (0..1), genes?: { id: value } (over the lineage's), site: { heat, wet, season, fertility, sand, clay, stones,
	//   depth, wind, slope, upset, bank } }: throws Refused on what isn't one
	void set(const Json::Value &intent);
	// The world's lineages, what the site comes to, and the plant's traits and blocks.
	Json::Value state() const;
	Scene::Boxes boxes() const;
	Scene::Ground ground() const;
	// Whether it's grown again since this was last asked.
	bool takeGrown()
	{
		const bool grown = m_drawn != m_version;
		m_drawn = m_version;
		return grown;
	}
	// How far back shows all of it.
	float distance() const;

private:
	void regrow();

	std::string m_world = "terra";
	int m_chosen = 0;
	u32 m_seed = 1;
	flora::Site m_site;
	flora::Life m_life;
	std::map<int, flora::fix> m_genes; // set by hand, by Gene
	std::vector<flora::Lineage> m_lineages;
	flora::Traits m_traits;
	flora::Plant m_plant;
	u64 m_version = 0, m_drawn = 0;
};

} // namespace shell
