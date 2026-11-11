// Standalone terragen (no engine): which places to probe, and what each
// probed column should be. Built and run by scripts/terragen-check.sh.
//   predict <code> <points-out>       → the places to probe, a direction per
//                                       line (x y z, 2^30 a unit)
//   predict --columns <code> <probed> → PREDICT lines for each MAP line: the
//                                       place of the node the probe found
//   predict --terra <code> <probed>   → what core.terra should have said, for
//                                       each TERRA line's place and clock
// Places: a grid of latitudes and longitudes, plus river channels, lake
// centres and volcanoes. The probe reports each node's own place
// (core.terra.place), and that's what's predicted: exactly the same place.
#include "terragen/calendar.h"
#include "terragen/sun.h"
#include "terragen/geology.h"
#include "terragen/world.h"
#include "terragen/sphere.h"
#include "terragen/grid.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
using namespace terragen;

static double real(fix v)
{
	return (double)v / ONE;
}

static double degrees(fix radians)
{
	return real(radians) * 180.0 / M_PI;
}

// The place each MAP line's probe id found: "MAP id x y z".
static std::map<int, Place> places(const char *probed)
{
	std::map<int, Place> out;
	FILE *in = std::fopen(probed, "r");
	if (!in)
		throw std::runtime_error(std::string("predict: can't read ") + probed);
	char line[512];
	while (std::fgets(line, sizeof line, in)) {
		const char *at = std::strstr(line, "MAP ");
		int id;
		Place d;
		if (at && std::sscanf(at, "MAP %d %d %d %d", &id, &d.x, &d.y, &d.z) == 4)
			out[id] = d;
	}
	std::fclose(in);
	return out;
}

// Mirrors src/script/lua_api/l_terra.cpp from the clock the probe read.
static int terra(const char *code, const char *probed)
{
	World w(WorldCode::decode(code));
	const std::map<int, Place> at = places(probed);
	FILE *in = std::fopen(probed, "r");
	char line[512];
	while (std::fgets(line, sizeof line, in)) {
		const char *found = std::strstr(line, "TERRA ");
		int id;
		unsigned day_count;
		double tod;
		if (!found || std::sscanf(found, "TERRA %d %u %lf", &id, &day_count, &tod) != 3)
			continue;
		const Place &d = at.at(id);
		fix hour = (fix)((float)tod * ONE);
		Calendar c = calendar(w.code().year, day_count, hour);
		fix latitude = latitudeOf(d.dir());
		fix decl = declination(w.code().tilt, c.date);
		Column column = w.column(d, w.sunlight(d, c.date));
		Ground gr = w.ground(d, w.sunlight(d, std::nullopt));
		std::string province = PROVINCE_NAMES[(int)w.strata(d, gr).province];
		std::replace(province.begin(), province.end(), ' ', '_');
		std::printf("TERRA %d %u %d %d %.6f %.6f %.6f %.6f %.6f %s %s %.6f %s\n", id, day_count,
				c.year + 1, c.day + 1, real(c.date), degrees(latitude), degrees(decl),
				degrees(elevation(latitude, decl, hour)), real(dayShare(latitude, decl)),
				SEASON_NAMES[(int)season(c.date, latitude)], BIOME_NAMES[(int)column.biome],
				real(w.temperature(d, c.date, hour)), province.c_str());
	}
	std::fclose(in);
	return 0;
}

static const int water_level = 1; // the engine's default

// soil under each biome (games/terrafirma/mods/tf_world/biomes.lua): its
// top's depth (none if plain stone), its filler's (SOIL: the column's soil,
// as deep as terragen says) and its riverbed's (SOIL: the column's soil, all
// of it, no filler under it). Plain stone isn't laid: the bedrock shows.
static const int SOIL = -1;
struct Laid
{
	int top, filler, bed;
};
static Laid laid(Biome b)
{
	switch (b) {
	case Biome::Deep: case Biome::SeaIce: return { 1, 2, SOIL };
	case Biome::Ocean: return { 1, 3, SOIL };
	case Biome::Beach: return { 2, 2, SOIL };
	case Biome::Desert: return { 2, 6, SOIL };
	case Biome::Rock: return { 0, 0, SOIL };
	case Biome::Snow: return { 1, 0, SOIL };
	case Biome::Regolith: case Biome::Mare: case Biome::Dust: case Biome::Sulfur: return { 2, 0, 2 };
	case Biome::SaltFlat: return { 1, 4, 2 };
	case Biome::Marsh: return { 2, 3, 2 };
	default: return { 1, SOIL, SOIL }; // the living land: grass over the column's soil
	}
}

// How deep the generator lays soil here, and (under water, on a bed of the
// column's soil) the block its bed's top is (terra_column.cpp)
static int soil(const World &w, const Place &at, const Ground &gr, bool bed, std::string &bed_top)
{
	const Laid l = laid(gr.column.biome);
	const Soil s = w.soil(at, gr, w.strata(at, gr));
	const int deep = std::max(1, s.depth);
	const int filler = l.filler == SOIL ? deep : l.filler;
	if (!bed)
		return l.top + filler;
	if (l.bed != SOIL)
		return l.bed + filler;
	const Soil top = soilLayer(s, 0);
	const int stony = soilStony(top.stones);
	bed_top = std::string("tf_world:soil_") + SOIL_PARENT_NAMES[(int)soilParent(top.parent)] +
			(stony ? std::string("_") + SOIL_STONY_NAMES[stony] : "");
	return deep;
}

// What the generator makes at a place: its surface, water and biome, and the
// block 20 below the surface, whose rock, ore and cave are the world
// model's at that height.
static void predictColumn(const World &w, int id, const Place &at)
{
	const char *kinds[] = { "dry", "sea", "lake", "river", "lava" };
	Ground gr = w.ground(at, w.sunlight(at, std::nullopt));
	int terrain = water_level + gr.terrain;
	int water = gr.kind == Water::None ? -9999 : water_level + gr.water;
	bool bed = (gr.kind == Water::River || gr.kind == Water::Lake) && gr.water > gr.terrain;
	std::string bed_top = "-";
	int depth = 20, below = depth - soil(w, at, gr, bed, bed_top);
	Strata st = w.strata(at, gr);
	Rock rock = w.rock(st, below, terrain - depth);
	Cave cave = w.cave(st, rock, depth, terrain - depth, gr.kind != Water::None && gr.kind != Water::Lava);
	Ore ore = w.ore(st, rock, below, terrain - depth);
	std::string deep = cave == Cave::Air ? "air" : cave == Cave::Water ? "tf_world:water_source"
			: cave == Cave::Lava ? "tf_world:lava_source"
			: ore != Ore::None ? std::string("tf_world:ore_") + ORE_NAMES[(int)ore]
			: std::string("tf_world:") + ROCK_NAMES[(int)rock];
	std::printf("PREDICT %d %d %d %s %s %s %s\n", id, terrain, water, kinds[(int)gr.kind], BIOME_NAMES[(int)gr.column.biome], deep.c_str(),
			bed_top.c_str());
}

int main(int argc, char **argv)
{
	if (std::strcmp(argv[1], "--terra") == 0)
		return terra(argv[2], argv[3]);
	if (std::strcmp(argv[1], "--columns") == 0) {
		World w(WorldCode::decode(argv[2]));
		for (const auto &[id, d] : places(argv[3]))
			predictColumn(w, id, d);
		return 0;
	}
	World w(WorldCode::decode(argv[1]));
	const Grid &g = w.grid();
	const Sphere &sphere = gridSphere();

	std::vector<Dir> points;
	// a grid of latitudes and longitudes, the poles too
	for (int i = -2; i <= 2; i++)
		for (int j = -2; j <= 2; j++)
			points.push_back(dirAt(mul(PI, ratio(j, 4)) * 85 / 90, mul(PI, ratio(i, 3))));
	points.push_back(dirAt(PI / 2, 0));
	points.push_back(dirAt(-PI / 2, ratio(1, 3)));
	// the centres of some river and lake cells: a river runs through its cell's centre
	int rivers = 0, lakes = 0;
	for (int32_t k = 0; k < g.size(); k += 97) {
		if (g.river(k) && rivers < 12) { points.push_back(sphere.centre(k)); rivers++; }
		else if (g.lake[k] && lakes < 6) { points.push_back(sphere.centre(k)); lakes++; }
	}
	// a couple of active volcanoes: the crater (lava) and a flank, half its radius north
	int volcanoes = 0;
	for (const Volcano &v : g.volcanoes) {
		if (!v.active || volcanoes == 2)
			continue;
		points.push_back(v.at);
		points.push_back(dirAt(latitudeOf(v.at) + w.angle(v.radius / 2), longitudeOf(v.at)));
		volcanoes++;
	}

	FILE *out = std::fopen(argv[2], "w");
	for (const Dir &d : points) {
		const Place p(d);
		std::fprintf(out, "%d %d %d\n", p.x, p.y, p.z);
	}
	std::fclose(out);
}
