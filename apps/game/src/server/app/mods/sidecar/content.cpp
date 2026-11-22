#include "src/server/app/mods/sidecar/content.h"

#include "src/server/app/mods/sidecar/msgpack.h"
#include "src/server/app/mods/sidecar/ops.h"
#include "src/server/app/mods/sidecar/sidecar.h"

namespace sidecar::content {

Content read(const mp::Value &p)
{
	Content content;
	try {
		if (p.at("protocol").asInt() != PROTOCOL)
			throw Error("it speaks protocol " + std::to_string(p.at("protocol").asInt()) +
					", this engine " + std::to_string(PROTOCOL) + ": the engine and terra are from different builds");
		for (const mp::Value &b : p.at("blocks").asArray()) {
			Content::Block block;
			block.name = b.at("name").asString();
			block.description = b.at("description").asString();
			const auto &tiles = b.at("tiles").asArray();
			if (tiles.size() != 6)
				throw Error(block.name + ": a block has six faces");
			for (size_t i = 0; i < 6; i++)
				block.tiles[i] = tiles[i].asString();
			block.matter = b.at("matter").asString();
			block.share = b.at("share").asNumber();
			content.blocks.push_back(std::move(block));
		}
		for (const mp::Value &i : p.at("items").asArray())
			content.items.push_back({ i.at("name").asString(), i.at("description").asString(), i.at("icon").asString() });
		for (const mp::Value &m : p.at("media").asArray())
			content.media.push_back({ m.at("name").asString(), m.at("path").asString() });
	} catch (const mp::Error &e) {
		throw Error(std::string("its content isn't what the engine reads: ") + e.what());
	}
	return content;
}

std::string fingerprint(const Content &content)
{
	std::string out;
	for (const auto &b : content.blocks) {
		out += b.name + '\n' + b.description + '\n' + b.matter + '\n' + std::to_string(b.share) + '\n';
		for (const auto &t : b.tiles)
			out += t + '\n';
	}
	for (const auto &i : content.items)
		out += i.name + '\n' + i.description + '\n' + i.icon + '\n';
	for (const auto &m : content.media)
		out += m.name + '\n' + m.path + '\n';
	return out;
}

} // namespace sidecar::content
