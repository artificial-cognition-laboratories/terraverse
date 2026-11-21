#include "ledger.h"
#include "sidecar.h"
#include "src/shared/platform/system/filesys.h"
#include <json/json.h>
#include <sstream>

namespace sidecar {

Ledger::Ledger(Opts o) : m_path(o.world + DIR_DELIM "mod_blocks.json") {}

void Ledger::record(const Content &content) const
{
	Blocks blocks = read();
	const size_t before = [&] { size_t n = 0; for (auto &[p, names] : blocks) n += names.size(); return n; }();
	for (const Content::Block &b : content.blocks) {
		const size_t colon = b.name.find(':');
		if (colon == std::string::npos)
			throw Error(b.name + ": a mod's block is named prefix:name");
		blocks[b.name.substr(0, colon)].insert(b.name);
	}
	size_t after = 0;
	for (auto &[p, names] : blocks)
		after += names.size();
	if (after == before && fs::PathExists(m_path))
		return; // nothing new: leave the file as it was

	Json::Value out(Json::objectValue);
	for (const auto &[prefix, names] : blocks) {
		Json::Value list(Json::arrayValue);
		for (const std::string &n : names)
			list.append(n);
		out[prefix] = list;
	}
	Json::StreamWriterBuilder w;
	w["indentation"] = "  ";
	if (!fs::safeWriteToFile(m_path, Json::writeString(w, out) + "\n"))
		throw Error("can't write " + m_path);
}

Ledger::Blocks Ledger::read() const
{
	Blocks blocks;
	std::string text;
	if (!fs::PathExists(m_path))
		return blocks;
	if (!fs::ReadFile(m_path, text))
		throw Error("can't read " + m_path);
	Json::Value data;
	Json::CharReaderBuilder reader;
	std::string errors;
	std::istringstream in(text);
	if (!Json::parseFromStream(reader, in, &data, &errors) || !data.isObject())
		throw Error(m_path + " isn't a ledger of blocks: " + errors);
	for (const std::string &prefix : data.getMemberNames()) {
		if (!data[prefix].isArray())
			throw Error(m_path + ": " + prefix + " isn't a list of blocks");
		for (const Json::Value &n : data[prefix]) {
			if (!n.isString())
				throw Error(m_path + ": " + prefix + " has a block that isn't a name");
			blocks[prefix].insert(n.asString());
		}
	}
	return blocks;
}

} // namespace sidecar
