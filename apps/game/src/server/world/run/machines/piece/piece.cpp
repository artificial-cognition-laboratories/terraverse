// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/server/world/run/machines/piece/piece.h"
#include "src/server/world/run/machines/piece/header.h"

#include "src/shared/base/exceptions.h"
#include "src/shared/base/gamedef.h"
#include "src/shared/map/block/mapblock.h"
#include "src/shared/map/block/mapsector.h"
#include "src/shared/game/content/nodedef.h"
#include "src/shared/map/raycast.h"
#include "src/shared/util/math/numeric.h"
#include "src/shared/util/data/serialize.h"
#include <map>
#include <sstream>
#include <tuple>

namespace piece {

namespace {

// TFP1: the palette and the nodes. TFP2 (sprint 48) had an id and a ghost
// flag first, now the object's (piece::Object): read past.
const char MAGIC[] = "TFP1";
const char MAGIC_2[] = "TFP2";

// The piece's grid: its map block, and a block of air round it every way
// (what light spreading and the tools read past its sides); air.
class Grid : public Map
{
public:
	explicit Grid(IGameDef *gamedef) : Map(gamedef)
	{
		for (s16 bx = -1; bx <= 1; bx++)
			for (s16 bz = -1; bz <= 1; bz++) {
				MapSector *sector = new MapSector(this, v2s16(bx, bz), gamedef);
				m_sectors[v2s16(bx, bz)] = sector;
				for (s16 by = -1; by <= 1; by++) {
					MapBlock *block = sector->createBlankBlock(by);
					for (s16 x = 0; x < MAP_BLOCKSIZE; x++)
						for (s16 y = 0; y < MAP_BLOCKSIZE; y++)
							for (s16 z = 0; z < MAP_BLOCKSIZE; z++)
								block->setNodeNoCheck({ x, y, z }, MapNode(CONTENT_AIR, LIGHT_SUN, 0));
				}
			}
	}
};

struct Entry
{
	v3s16 p;
	u16 name;
	u8 param2;
	std::optional<shape::Shape> shape;
};

struct Parsed
{
	std::vector<std::string> names;
	std::vector<Entry> entries;
};

// The text's palette and nodes; throws on anything that isn't a piece.
Parsed parse(const std::string &text)
{
	std::istringstream is(text, std::ios::binary);
	char magic[4];
	is.read(magic, 4);
	const std::string m(magic, is.gcount());
	if (m == std::string(MAGIC_2, 4)) {
		readU64(is);
		readU8(is);
	} else if (m != std::string(MAGIC, 4)) {
		throw SerializationError("piece: not a piece");
	}
	Parsed out;
	std::vector<std::string> &names = out.names;
	std::vector<Entry> &entries = out.entries;
	names.resize(readU16(is));
	for (std::string &name : names)
		name = deSerializeString16(is);
	entries.resize(readU16(is));
	for (Entry &e : entries) {
		// one at a time: arguments aren't read in order
		const u8 x = readU8(is), y = readU8(is), z = readU8(is);
		e.p = v3s16(x, y, z);
		if (e.p.X >= SIDE || e.p.Y >= SIDE || e.p.Z >= SIDE)
			throw SerializationError("piece: a node off its grid");
		e.name = readU16(is);
		if (e.name >= names.size())
			throw SerializationError("piece: a node not in the palette");
		e.param2 = readU8(is);
		if (readU8(is))
			e.shape = shape::Shape::deSerialize(is);
	}
	return out;
}

} // namespace

Piece::Piece(IGameDef *gamedef) : m_gamedef(gamedef), m_map(std::make_unique<Grid>(gamedef)) {}

Piece::~Piece() = default;

void Piece::read(const std::string &text)
{
	const Parsed parsed = parse(text);
	const auto &names = parsed.names;
	const auto &entries = parsed.entries;

	const NodeDefManager *ndef = m_gamedef->ndef();
	std::vector<content_t> ids;
	for (const std::string &name : names) {
		content_t id;
		if (!ndef->getId(name, id))
			throw SerializationError("piece: no node \"" + name + "\"");
		ids.push_back(id);
	}
	m_map = std::make_unique<Grid>(m_gamedef);
	for (const Entry &e : entries) {
		m_map->setNode(e.p, MapNode(ids[e.name], 0, e.param2));
		if (e.shape)
			m_map->setShape(e.p, *e.shape);
	}
}

std::string Piece::write() const
{
	const NodeDefManager *ndef = m_gamedef->ndef();
	std::vector<std::string> names;
	std::map<content_t, u16> palette;
	std::vector<Entry> entries;
	each([&](v3s16 p, MapNode n, const shape::Shape *s) {
		auto [it, fresh] = palette.try_emplace(n.getContent(), u16(names.size()));
		if (fresh)
			names.push_back(ndef->get(n).name);
		entries.push_back({ p, it->second, n.getParam2(), s ? std::optional<shape::Shape>(*s) : std::nullopt });
	});
	std::ostringstream os(std::ios::binary);
	os.write(MAGIC, 4);
	writeU16(os, u16(names.size()));
	for (const std::string &name : names)
		os << serializeString16(name);
	writeU16(os, u16(entries.size()));
	for (const Entry &e : entries) {
		writeU8(os, u8(e.p.X));
		writeU8(os, u8(e.p.Y));
		writeU8(os, u8(e.p.Z));
		writeU16(os, e.name);
		writeU8(os, e.param2);
		writeU8(os, e.shape ? 1 : 0);
		if (e.shape)
			e.shape->serialize(os);
	}
	return os.str();
}

void Piece::shift(v3s16 by)
{
	std::vector<std::tuple<v3s16, MapNode, std::optional<shape::Shape>>> nodes;
	each([&](v3s16 p, MapNode n, const shape::Shape *s) {
		const v3s16 q = p + by;
		if (q.X < 0 || q.Y < 0 || q.Z < 0 || q.X >= SIDE || q.Y >= SIDE || q.Z >= SIDE)
			throw BaseException("Piece::shift: off its grid");
		nodes.emplace_back(q, n, s ? std::optional<shape::Shape>(*s) : std::nullopt);
	});
	m_map = std::make_unique<Grid>(m_gamedef);
	for (const auto &[q, n, s] : nodes) {
		m_map->setNode(q, n);
		if (s)
			m_map->setShape(q, *s);
	}
}

bool Piece::empty() const
{
	bool any = false;
	each([&](v3s16, MapNode, const shape::Shape *) { any = true; });
	return !any;
}

std::optional<Piece::Hit> Piece::raycast(const core::line3d<f32> &line) const
{
	std::optional<Hit> best;
	f32 best_d = 0;
	std::vector<aabb3f> boxes;
	each([&](v3s16 p, MapNode, const shape::Shape *) {
		boxes.clear();
		if (!m_map->getShapedBoxes(p, boxes))
			boxes.emplace_back(-BS / 2, -BS / 2, -BS / 2, BS / 2, BS / 2, BS / 2);
		const v3f centre = intToFloat(p, BS);
		for (const aabb3f &b : boxes) {
			v3f point, normal;
			if (!boxLineCollision(b, line.start - centre, line.getVector(), &point, &normal))
				continue;
			point += centre;
			const f32 d = (point - line.start).getLengthSQ();
			if (!best || d < best_d) {
				best = Hit{ p, point, floatToInt(normal, 1) };
				best_d = d;
			}
		}
	});
	return best;
}

std::optional<aabb3f> boxOf(const std::string &text)
{
	const Parsed parsed = parse(text);
	std::optional<aabb3f> out;
	for (const Entry &e : parsed.entries) {
		const v3f c(e.p.X, e.p.Y, e.p.Z);
		const aabb3f cell(c - v3f(0.5f), c + v3f(0.5f));
		if (out)
			out->addInternalBox(cell);
		else
			out = cell;
	}
	return out;
}

} // namespace piece
