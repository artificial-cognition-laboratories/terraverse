// A universe on disk, made from a world code: the game's world folder, and the map
// settings that make it that star system (sprint 119). Made here for both the main menu's
// new worlds (shell/worlds.cpp) and a TypeScript server's first boot (terra host).
#pragma once

#include <string>

struct SubgameSpec;

namespace universe {

// Make the world folder at `path` for `game`, as the star system `code` is. Throws
// BaseException if it can't be written, terragen::CodeError if the code isn't one.
void make(const std::string &path, const std::string &name, const SubgameSpec &game, const std::string &code);

// The world code the universe at `path` was made from, or "" if it wasn't made from one.
std::string codeOf(const std::string &path);

} // namespace universe
