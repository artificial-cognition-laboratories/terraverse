// Terra Firma: what TypeScript mods declare, registered beside the engine's own (sprint 119).
// See registry.h.
#include "src/server/game/registry.h"

#include "src/shared/base/config.h"
#if CHECK_CLIENT_BUILD()
#include "src/client/render/map/mesh/node_visuals.h" // ~NodeVisuals, as ContentFeatures moves
#endif

#include "src/server/game/defs.h"
#include "src/shared/base/exceptions.h"
#include "src/server/world/run/rules/matter.h"
#include "library/matter/matter/matter.h"
#include "src/server/app/server.h"
#include "src/server/app/mods/sidecar/sidecar.h"
#include <stdexcept>

namespace {

void block(Server *server, const sidecar::Content::Block &b);
void item(Server *server, const sidecar::Content::Item &i);

} // namespace

void registerModContent(Server *server, const sidecar::Content &content)
{
	for (const sidecar::Content::Block &b : content.blocks)
		block(server, b);
	for (const sidecar::Content::Item &i : content.items)
		item(server, i);
}

namespace {

void block(Server *server, const sidecar::Content::Block &b)
{
	const matter::Matter &table = serverMatter(server).table;
	const matter::Substance *substance = nullptr;
	for (const matter::Substance &s : table.substances())
		if (s.id == b.matter)
			substance = &s;
	Json::Value def;
	def["description"] = b.description;
	Json::Value tiles(Json::arrayValue);
	for (const std::string &t : b.tiles)
		tiles.append(t);
	def["tiles"] = tiles;
	// what you can see through is drawn as glass (a rock, of no one substance, isn't)
	if (substance && substance->transparent) {
		def["drawtype"] = "glasslike";
		def["use_texture_alpha"] = "blend";
		def["paramtype"] = "light";
		def["sunlight_propagates"] = true;
	}
	// TEMP(2026-11-18): every block digs like stone until its matter decides how
	def["groups"]["cracky"] = 3;
	def["_matter"]["matter"] = b.matter;
	def["_matter"]["share"] = b.share;
	if (server->getItemDefManager()->isKnown(b.name))
		throw BaseException("TypeScript mods: " + b.name + " is already a block or item");
	addBlock(server, b.name, def);
}

// A craft item: its icon in the inventory, and (Luanti's way) the same icon extruded
// into a thick sprite in the hand, so a 16 × 16 silhouette reads as a solid tool.
void item(Server *server, const sidecar::Content::Item &i)
{
	if (server->getItemDefManager()->isKnown(i.name))
		throw BaseException("TypeScript mods: " + i.name + " is already a block or item");
	Json::Value def;
	def["description"] = i.description;
	def["inventory_image"] = i.icon;
	def["wield_image"] = i.icon;
	try {
		server->getWritableItemDefManager()->registerItem(defs::item(i.name, def));
	} catch (const std::invalid_argument &e) {
		throw BaseException(std::string("TypeScript mods: ") + e.what());
	}
}

} // namespace
