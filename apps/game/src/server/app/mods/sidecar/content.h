// What TypeScript mods declare while they load (sprint 119): their blocks, their items and
// the media both draw from, as the sidecar sends it once, read here into a strict shape.
#pragma once

#include <array>
#include <string>
#include <vector>

namespace sidecar {

namespace mp { class Value; }

// What the mods declared while they loaded. Sealed: a restarted sidecar must declare
// the same, or its new content waits for a restart.
struct Content {
	struct Block {
		std::string name, description;
		std::array<std::string, 6> tiles;  // top, bottom, right, left, back, front: media names
		std::string matter;
		double share;
	};
	// A hand-sized thing: its icon (a media name) is drawn in the inventory and extruded in the hand.
	struct Item {
		std::string name, description, icon;
	};
	struct Media {
		std::string name, path;
	};
	std::vector<Block> blocks;
	std::vector<Item> items;
	std::vector<Media> media;
};

namespace content {

// The sidecar's content payload, protocol checked. Throws Error saying what isn't right.
Content read(const mp::Value &payload);

// What a restarted sidecar's content must match: its declarations, in order.
std::string fingerprint(const Content &content);

} // namespace content

} // namespace sidecar
