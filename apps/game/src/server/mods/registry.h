// Terra Firma: what the world's made of, registered natively (sprint 119). The engine's
// own blocks and items are data (assets/world/*.json) and what the matter table says
// (a block of every substance); a TypeScript mod's blocks join them. Registered before
// the world starts, so the node definitions sent to players, the generator, the world's
// matter and machines all see one set.
#pragma once

#include <string>

class Server;
namespace sidecar { struct Content; }

// The engine's own: the hand, the world's blocks and soils, the matter's, the planet's,
// their aliases; and where the world's generated, its biomes and tree species. Throws
// BaseException naming what's wrong in the data.
void registerContent(Server *server);

// A TypeScript mod's blocks (sidecar::Content): each a block of its substance, drawn as
// glass where that's see-through. Throws BaseException for one already registered.
void registerModContent(Server *server, const sidecar::Content &content);

// The text of one of the engine's world data files (<share>/world/<file>).
std::string worldData(const std::string &file);
