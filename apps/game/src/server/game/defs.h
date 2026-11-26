// Terraverse: blocks and items as data (sprint 119). One JSON definition says what a
// block is (assets/world/blocks.json, a TypeScript mod's block, a substance's); this reads
// it into what the engine holds: its item and, for a block, its node. Only the fields
// content uses are read, and any other is refused, so a typo fails loudly rather than
// being quietly ignored.
#pragma once

#include "src/shared/game/items/itemdef.h"
#include "src/shared/game/content/nodedef.h"
#include <json/json.h>
#include <string>

namespace defs {

struct Block
{
	ItemDefinition item;
	ContentFeatures node;
};

// A block called `name` (its definition's fields as blocks.json writes them). Throws
// std::invalid_argument naming the block and the field.
Block block(const std::string &name, const Json::Value &def);

// An item that isn't a block: the hand ("" with "type": "none"), or a thing.
ItemDefinition item(const std::string &name, const Json::Value &def);

} // namespace defs
