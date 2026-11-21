// What each mod has put in a world's definitions: what removing it would leave behind.
// Kept beside the world (<world>/mod_blocks.json), by mod prefix, and only ever grows:
// a block registered once may be in the map anywhere.
#pragma once

#include <map>
#include <set>
#include <string>

namespace sidecar {

struct Content;

class Ledger {
public:
	struct Opts {
		std::string world; // the world's folder
	};
	explicit Ledger(Opts o);

	using Blocks = std::map<std::string, std::set<std::string>>; // prefix → block names

	// This run's blocks, added to those on record. Throws Error if it can't be written.
	void record(const Content &content) const;
	// What's on record; empty for a world that has never run mods. Throws Error if unreadable.
	Blocks read() const;

private:
	std::string m_path;
};

} // namespace sidecar
