// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "items.h"
#include "client/client.h"
#include "client/texturesource.h"
#include "exceptions.h"
#include "filesys.h"
#include "itemdef.h"
#include "log.h"
#include "nodedef.h"
#include "util/string.h"
#include <IImage.h>
#include <IVideoDriver.h>
#include <cstring>

namespace shell {

// Drawtypes that read as a solid block and get an isometric cube icon;
// everything else (plants, torches, rails) shows its first tile flat.
static bool drawsAsCube(NodeDrawType drawtype)
{
	switch (drawtype) {
	case NDT_NORMAL:
	case NDT_LIQUID:
	case NDT_GLASSLIKE:
	case NDT_GLASSLIKE_FRAMED:
	case NDT_ALLFACES:
	case NDT_ALLFACES_OPTIONAL:
		return true;
	default:
		return false;
	}
}

// Inside [inventorycube parts, '^' must be written as '&'.
static std::string cubePart(std::string texture)
{
	str_replace(texture, '^', '&');
	return texture;
}

const Json::Value &Items::describe(const std::string &name)
{
	auto found = m_described.find(name);
	if (found != m_described.end())
		return found->second;

	const ItemDefinition &def = m_opts.client->idef()->get(name);
	Json::Value info;
	info["description"] = def.description.empty() ? name : def.description;
	// kg each, as the game weighs it (tf_craft carry.lua: group mass_g, grams); null if unweighed
	const auto grams = def.groups.find("mass_g");
	info["mass"] = grams == def.groups.end() ? Json::Value() : Json::Value(grams->second / 1000.0);

	std::string texture = iconTexture(def);
	if (texture.empty()) {
		info["icon"] = Json::nullValue; // nothing to draw, e.g. the empty hand
	} else {
		std::string file = name;
		str_replace(file, ':', '.');
		file += ".png";
		writeIcon(texture, m_opts.dir + DIR_DELIM + file);
		info["icon"] = "terra://item/" + file + "?s=" + m_opts.session;
	}
	return m_described.emplace(name, info).first->second;
}

Json::Value Items::catalog()
{
	std::set<std::string> names;
	m_opts.client->idef()->getAll(names);
	Json::Value result(Json::arrayValue);
	for (const std::string &name : names) {
		const ItemDefinition &def = m_opts.client->idef()->get(name);
		if (name.empty() || def.name != name || def.type == ITEM_NONE || def.description.empty() ||
				name.ends_with("_flowing") ||
				def.groups.count("not_in_creative_inventory"))
			continue;
		Json::Value item = describe(name);
		item["item"] = name;
		item["stackMax"] = def.stack_max;
		if (def.groups.count("creative_matter"))
			item["category"] = "matter"; // a block of a substance (tf_matter)
		else if (def.groups.count("creative_nature") || def.groups.count("tree") ||
				def.groups.count("leaves") || def.groups.count("plant") ||
				def.groups.count("crumbly") || def.groups.count("liquid"))
			item["category"] = "nature";
		else if (def.groups.count("creative_functional"))
			item["category"] = "functional";
		else if (def.type == ITEM_NODE)
			item["category"] = "building";
		else if (def.type == ITEM_TOOL)
			item["category"] = "tools";
		else
			item["category"] = "items";
		result.append(item);
	}
	return result;
}

std::string Items::iconTexture(const ItemDefinition &def) const
{
	if (!def.inventory_image.name.empty()) {
		std::string texture = def.inventory_image.name;
		if (!def.inventory_overlay.name.empty())
			texture += "^(" + def.inventory_overlay.name + ")";
		return texture;
	}
	if (def.type != ITEM_NODE)
		return "";

	const ContentFeatures &node = m_opts.client->ndef()->get(def.name);
	// tiledef order: +Y, -Y, +X, -X, +Z, -Z. The cube shows top, front (-Z), right (+X).
	if (!drawsAsCube(node.drawtype))
		return node.tiledef[0].name;
	return "[inventorycube{" + cubePart(node.tiledef[0].name) + "{" +
			cubePart(node.tiledef[5].name) + "{" + cubePart(node.tiledef[2].name);
}

void Items::writeIcon(const std::string &texture, const std::string &path) const
{
	video::ITexture *tex = m_opts.client->tsrc()->getTexture(texture);
	if (!tex)
		throw BaseException("shell: could not generate icon texture \"" + texture + "\"");

	core::dimension2du size = tex->getSize();
	video::IImage *image = m_opts.driver->createImage(tex->getColorFormat(), size);
	const u8 *src = static_cast<const u8 *>(tex->lock(video::ETLM_READ_ONLY, 0, 0, video::ETLF_NONE));
	if (!src) {
		image->drop();
		throw BaseException("shell: could not read back icon texture \"" + texture + "\"");
	}
	u8 *dst = static_cast<u8 *>(image->getData());
	u32 row = std::min(image->getPitch(), tex->getPitch());
	for (u32 y = 0; y < size.Height; y++)
		std::memcpy(dst + y * image->getPitch(), src + y * tex->getPitch(), row);
	tex->unlock();

	fs::CreateAllDirs(fs::RemoveLastPathComponent(path));
	bool written = m_opts.driver->writeImageToFile(image, path.c_str());
	image->drop();
	if (!written)
		throw BaseException("shell: could not write icon " + path);
	verbosestream << "shell: icon " << path << " from \"" << texture << "\"" << std::endl;
}

} // namespace shell
