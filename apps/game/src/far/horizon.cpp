// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "horizon.h"
#include "exceptions.h"
#include "network/networkpacket.h"
#include <cstring>

namespace far {

static void putDouble(NetworkPacket &pkt, double v)
{
	u64 bits;
	std::memcpy(&bits, &v, sizeof bits);
	pkt << bits;
}

static double getDouble(NetworkPacket &pkt)
{
	u64 bits;
	pkt >> bits;
	double v;
	std::memcpy(&v, &bits, sizeof v);
	return v;
}

// a count that must match ours: the server's terragen is ours, or nothing is
static void expect(NetworkPacket &pkt, u8 ours, const char *what)
{
	u8 theirs;
	pkt >> theirs;
	if (theirs != ours)
		throw SerializationError(std::string("TOCLIENT_HORIZON: the server's ") + what + " don't match ours");
}

void write(NetworkPacket &pkt, const Horizon &h)
{
	const terra::Blocks &b = h.content.blocks;
	pkt << h.code << h.water_level;
	pkt << (u8)terragen::Biome::Count;
	for (const terra::BiomeBlocks &bb : b.biomes)
		pkt << bb.top << bb.filler << bb.riverbed << bb.water_top
			<< bb.depth_top << bb.depth_filler << bb.depth_riverbed << bb.depth_water_top;
	pkt << b.stone << b.water << b.lava << b.placer << b.soil;
	pkt << (u8)terragen::Rock::Count;
	for (content_t c : b.rock)
		pkt << c;
	pkt << (u8)terragen::Ore::Count;
	for (content_t c : b.ore)
		pkt << c;

	pkt << (u16)h.content.species.size();
	for (const terra::Species &s : h.content.species) {
		const terra::Niche &n = s.niche;
		pkt << s.name;
		for (double v : { n.heat[0], n.heat[1], n.wet[0], n.wet[1], n.season[0], n.season[1] })
			putDouble(pkt, v);
		pkt << n.seasonal << n.bank << n.shore;
		pkt << (u16)s.on.size();
		for (content_t c : s.on)
			pkt << c;
		pkt << s.trunk << s.leaves;
		const terra::Shape &sh = s.shape;
		pkt << sh.axiom << sh.rules_a << sh.rules_b << sh.rules_c << sh.rules_d << sh.trunk_type
			<< sh.angle << sh.iterations << sh.random_level << sh.thin_branches;
	}
}

Horizon read(NetworkPacket &pkt)
{
	Horizon h;
	terra::Blocks &b = h.content.blocks;
	pkt >> h.code >> h.water_level;
	expect(pkt, (u8)terragen::Biome::Count, "biomes");
	for (terra::BiomeBlocks &bb : b.biomes)
		pkt >> bb.top >> bb.filler >> bb.riverbed >> bb.water_top
			>> bb.depth_top >> bb.depth_filler >> bb.depth_riverbed >> bb.depth_water_top;
	pkt >> b.stone >> b.water >> b.lava >> b.placer >> b.soil;
	expect(pkt, (u8)terragen::Rock::Count, "rocks");
	for (content_t &c : b.rock)
		pkt >> c;
	expect(pkt, (u8)terragen::Ore::Count, "ores");
	for (content_t &c : b.ore)
		pkt >> c;

	u16 count;
	pkt >> count;
	h.content.species.resize(count);
	for (terra::Species &s : h.content.species) {
		terra::Niche &n = s.niche;
		pkt >> s.name;
		for (double *v : { &n.heat[0], &n.heat[1], &n.wet[0], &n.wet[1], &n.season[0], &n.season[1] })
			*v = getDouble(pkt);
		pkt >> n.seasonal >> n.bank >> n.shore;
		u16 on;
		pkt >> on;
		s.on.resize(on);
		for (content_t &c : s.on)
			pkt >> c;
		pkt >> s.trunk >> s.leaves;
		terra::Shape &sh = s.shape;
		pkt >> sh.axiom >> sh.rules_a >> sh.rules_b >> sh.rules_c >> sh.rules_d >> sh.trunk_type
			>> sh.angle >> sh.iterations >> sh.random_level >> sh.thin_branches;
	}
	return h;
}

} // namespace far
