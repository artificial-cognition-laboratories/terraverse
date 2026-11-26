// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irr_v3d.h"
#include <string>
#include <vector>

namespace workshop {

/*
	A blueprint: a design kept to be made again, free (sprint 48): its
	machines, each placed from where the blueprint is put down (one for
	now: a machine holds its own parts and joints, sprint 52).
*/
struct Blueprint
{
	struct Part
	{
		v3s16 offset; // nodes, from where it's put down
		std::string machine; // its text (Machine::write); an object's from before reads as one
	};
	std::vector<Part> parts;

	std::string write() const;
	// Throws SerializationError on text that isn't a blueprint.
	static Blueprint read(const std::string &text);
};

/*
	Each player's blueprints, by name: their parts library. Kept outside
	any world (sprint 74: <root>/<player>/<name>.tfb, root the user's
	parts folder), so what's made in one world is there in the next; a
	world's own from before (<world>/blueprints) are copied in as it opens,
	where the name's free. A name is 1..LONGEST_NAME of letters, digits,
	spaces, '-' and '_'.
*/
class Library
{
public:
	static constexpr size_t LONGEST_NAME = 32;

	// `world_path`: a world whose blueprints from before are adopted (empty: none).
	Library(const std::string &root, const std::string &world_path);

	static bool named(const std::string &name);
	// Throws std::invalid_argument on a name that isn't one, and
	// FileNotGoodException when it can't be written.
	void save(const std::string &player, const std::string &name, const Blueprint &blueprint);
	// Throws std::out_of_range when the player has none of that name.
	Blueprint load(const std::string &player, const std::string &name) const;
	// Throws std::out_of_range when the player has none of that name.
	void remove(const std::string &player, const std::string &name);
	// The player's, sorted.
	std::vector<std::string> names(const std::string &player) const;

private:
	std::string dir(const std::string &player) const;

	std::string m_root;
};

} // namespace workshop
