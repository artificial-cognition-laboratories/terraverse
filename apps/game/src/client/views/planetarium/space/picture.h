// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/space/milkyway/milkyway.h"
#include <memory>
#include <string>

/*
	The sky's picture (milkyway::), where it's kept. The full one ships with
	the game, baked while staging (`terraverse --bake-galaxy <dir>`): baked on
	a player's machine it took every core for seconds, and the first world
	made beside it took three times as long. A build without it (one not yet
	staged) bakes it into the cache as before.
*/
namespace planetarium::picture {

struct Size
{
	unsigned width, height;
};
// about 650 texels a radian: the dust sharp at 80° across a 4K screen
constexpr Size FULL{ 4096, 2048 }, QUICK{ 1024, 512 };

// Shipped or kept before, if it is
std::shared_ptr<const milkyway::Picture> find(Size size);
// Found, or baked (seconds, every core) and kept in the cache
std::shared_ptr<const milkyway::Picture> paint(Size size);
// The full one into `dir` to ship, unless it's there; other versions there are
// removed (throws if it can't be written)
void ship(const std::string &dir);

} // namespace planetarium::picture
