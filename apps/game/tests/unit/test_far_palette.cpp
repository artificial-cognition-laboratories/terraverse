// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tests/unit/test.h"

#include "src/client/views/game/far/baked.h"
#include "src/client/views/game/far/mesh.h"
#include "src/client/views/game/far/tiles.h"
#include "src/shared/core/exceptions.h"
#include "src/shared/platform/filesys.h"
#include "src/shared/map/light.h"
#include "src/shared/platform/porting.h"
#include <chrono>
#include <cmath>
#include <fstream>
#include <thread>

using namespace far;

/*
	The baked palette (far/baked.h) and the tiles drawn with it (far/tiles.h),
	with no game running: what the menu and the Planetarium have.
*/
class TestFarPalette : public TestBase {
public:
	TestFarPalette() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestFarPalette"; }

	void runTests(IGameDef *gamedef);

	void testBakedRoundTrip();
	void testShippedPaletteIsWhole();
	void testTilesForAnEye();
	void testSeaIsWater();
	void testTintPaintsTheLand();
	void testInsideTheCutIsFine();
};

static TestFarPalette g_test_instance;

void TestFarPalette::runTests(IGameDef *gamedef)
{
	set_light_curve(1.0f); // tiles are lit by it, as the client sets it
	TEST(testBakedRoundTrip);
	TEST(testShippedPaletteIsWhole);
	TEST(testTilesForAnEye);
	TEST(testSeaIsWater);
	TEST(testTintPaintsTheLand);
	TEST(testInsideTheCutIsFine);
}

namespace {

// Content with made-up ids, every block a look of its own colour; the soil
// tinted by a palette.
std::pair<Content, Palette> made()
{
	Content c;
	terra::Blocks &b = c.blocks;
	content_t next = 10;
	for (terra::BiomeBlocks &bb : b.biomes)
		bb = { next++, next++, next++, 11, 1, 3, 1, 0 };
	b.stone = next++;
	b.water = 11;
	b.lava = next++;
	b.placer = next++;
	b.soil = next++;
	for (auto &kinds : b.soils)
		for (content_t &c : kinds)
			c = next++;
	for (content_t &r : b.rock)
		r = next++;
	b.ore[0] = CONTENT_IGNORE;
	for (size_t i = 1; i < std::size(b.ore); i++)
		b.ore[i] = next++;
	terra::Species oak{ "oak", { { -0.123456789012345, 1 }, { -1, 1 }, true, { 0.25, 0.5 }, true, false }, {}, next, (content_t)(next + 1),
		{ "FFFFA", "[&&fff]", "", "", "", "single", 45, 1, 0, true } };
	next += 2;
	oak.on.push_back(b.biomes[0].top);
	c.species.push_back(oak);

	Palette p;
	for (content_t id : blocksIn(c)) {
		if (id == CONTENT_IGNORE || p.looks().count(id))
			continue;
		Palette::Look l{};
		for (int k = 0; k < 6; k++) {
			l.face[k] = video::SColorf(id / 256.0f, k / 6.0f, 0.1f + 1.0f / 3.0f, 1);
			l.tinted[k] = k % 2;
		}
		l.tints.assign(id == b.soil ? 256 : 1, video::SColorf(0.7f, 0.2f, 0.123456f, 1));
		if (id == b.soil)
			l.tints[200] = video::SColorf(0.1f, 0.9f, 0.3f, 1);
		l.over_black = video::SColorf(0.01f, 0.02f, 0.03f, 1);
		l.clarity = id == b.water ? 0.4f : 0;
		p.add(id, std::move(l));
	}
	return { c, p };
}

std::string nameOf(content_t id)
{
	return "test:b" + std::to_string(id);
}

// The game's own, as staged next to the binary.
Baked shipped()
{
	return Baked::load("terrafirma");
}

// Updates the tiles for an eye until every tile it wants is built (at least
// 50 updates). Bounded only against a hang: a deadline that's a budget made
// the test fail on a busy machine (every module runs side by side), and how
// fast tiles build is not what these tests check.
void settle(Tiles &tiles, const Eye &eye)
{
	const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(180);
	for (int i = 0; i < 50 || (tiles.pending() > 0 && std::chrono::steady_clock::now() < until); i++) {
		tiles.update(eye);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

} // namespace

// Written and read back, every role is the same block (by name) and every
// look the same, bit for bit.
void TestFarPalette::testBakedRoundTrip()
{
	const auto [content, palette] = made();
	Json::StreamWriterBuilder writer;
	writer["precision"] = 17;
	Json::Value json;
	std::istringstream text(Json::writeString(writer, Baked::write(content, palette, nameOf)));
	text >> json;
	const Baked b = Baked::read(json);
	const auto same = [&](content_t was, content_t now) {
		return was == CONTENT_IGNORE ? now == CONTENT_IGNORE : b.names.at(now) == nameOf(was);
	};
	const terra::Blocks &x = content.blocks, &y = b.content.blocks;
	UASSERT(same(x.stone, y.stone) && same(x.water, y.water) && same(x.soil, y.soil) && same(x.placer, y.placer));
	for (size_t i = 0; i < std::size(x.biomes); i++)
		UASSERT(same(x.biomes[i].top, y.biomes[i].top) && same(x.biomes[i].filler, y.biomes[i].filler) &&
				x.biomes[i].depth_filler == y.biomes[i].depth_filler);
	for (size_t i = 0; i < std::size(x.ore); i++)
		UASSERT(same(x.ore[i], y.ore[i]));
	const terra::Species &s = b.content.species.at(0);
	UASSERT(s.niche.heat[0] == -0.123456789012345 && s.niche.season[1] == 0.5 && s.niche.seasonal && s.niche.bank);
	UASSERT(same(content.species[0].trunk, s.trunk) && same(content.species[0].on[0], s.on.at(0)));
	UASSERT(s.shape.rules_a == "[&&fff]" && s.shape.angle == 45 && s.shape.thin_branches);
	for (const auto &[id, l] : palette.looks()) {
		content_t found = CONTENT_IGNORE;
		for (content_t i = 0; i < b.names.size(); i++)
			if (b.names[i] == nameOf(id))
				found = i;
		const Palette::Look &r = b.palette.look(found);
		for (int k = 0; k < 6; k++)
			UASSERT(r.face[k].r == l.face[k].r && r.face[k].g == l.face[k].g && r.face[k].b == l.face[k].b &&
					r.tinted[k] == l.tinted[k]);
		UASSERT(r.tints.size() == l.tints.size() && r.clarity == l.clarity && r.over_black.g == l.over_black.g);
		for (size_t i = 0; i < l.tints.size(); i++)
			UASSERT(r.tints[i].r == l.tints[i].r && r.tints[i].b == l.tints[i].b);
	}
	// a face takes its param2's tint
	const MapNode soil(b.content.blocks.soil, 0, 200);
	UASSERT(std::fabs(b.palette.face(soil, EAST).g - palette.face(MapNode(content.blocks.soil, 0, 200), EAST).g) == 0);
}

// The game's palette knows every block its content places.
void TestFarPalette::testShippedPaletteIsWhole()
{
	const Baked b = shipped();
	UASSERT(!b.content.species.empty());
	for (content_t id : blocksIn(b.content))
		if (id != CONTENT_IGNORE && id != CONTENT_AIR)
			b.palette.look(id); // throws if missing
}

// For an eye over a small planet, built from the game's palette with no
// game: tiles come, nothing past the eye's reach is drawn, and tiles near
// the eye are finer than those far off.
void TestFarPalette::testTilesForAnEye()
{
	const Baked b = shipped();
	const Horizon h{ "TF24.dGVycmE", 1, b.content };
	const planet::Atlas atlas = planet::Atlas::resident({ 300, 12, 80, 60, 0.95, 32 });
	Tiles tiles(atlas, planet::Atlas::HOME, h, b.palette, 2);
	const double sea = 300 + h.water_level + 1;
	const Eye eye{ { sea, 0, 0 }, 40, 0, 500, 2 };
	std::vector<Tiles::Drawn> drawn;
	settle(tiles, eye);
	UTEST(tiles.pending() == 0, "%zu tiles still being built", tiles.pending());
	drawn = tiles.drawn();
	UASSERT(!drawn.empty());
	const Tiling &g = tiles.tiling();
	s32 nearest = 99, farthest = -1;
	double near_d = 1e30, far_d = -1;
	for (const Tiles::Drawn &t : drawn) {
		const double d = std::hypot(t.middle.x * sea / g.radius() - eye.at.x, t.middle.y * sea / g.radius() - eye.at.y,
				t.middle.z * sea / g.radius() - eye.at.z) - g.diagonal(t.key) / 2;
		UTEST(d <= eye.range, "a tile %.0f blocks off is drawn past the eye's %.0f", d, eye.range);
		if (d < near_d) { near_d = d; nearest = t.key.level; }
		if (d > far_d) { far_d = d; farthest = t.key.level; }
	}
	UTEST(nearest < farthest, "the nearest tile (level %d) isn't finer than the farthest (%d)", nearest, farthest);
}


// A tile of sea, from the game's palette: its water's top is the water's
// colour seen over the floor, not black (it once was: lit by an unset light
// curve, 0 / 0).
void TestFarPalette::testSeaIsWater()
{
	const Baked b = shipped();
	Tile t;
	t.key = { 0, 0, 0, 0 };
	t.cells.resize((TILE + 2) * (TILE + 2));
	const MapNode floor(b.content.blocks.biomes[0].top), water(b.content.blocks.water);
	for (Cell &c : t.cells) {
		c = Cell{};
		c.ground = c.top_end = c.filler_end = -10;
		c.top = c.filler = c.rock = floor;
		c.water = 1;
		c.liquid = water;
	}
	const Mesh m = far::mesh(t, b.palette);
	// at least the water's own colour over black (light only adds the floor's)
	const video::SColor least = b.palette.look(water.getContent()).over_black.toSColor();
	size_t tops = 0;
	for (const Mesh::Part &p : m.parts)
		for (const far::Vertex &v : p.vertices)
			if (v.Normal.Y > 0.5f && v.Pos.Y > 0) { // a liquid's top: up, above the floor (at 1.5, the floor's at -9.5)
				tops++;
				UASSERT(v.Color.getRed() + v.Color.getGreen() + v.Color.getBlue() > 60);
				UASSERT(v.Color.getGreen() + 1 >= least.getGreen() && v.Color.getBlue() + 1 >= least.getBlue());
			}
	UASSERT(tops > 0);
}



// A layer over the land (Tiles::tint): every drawn vertex takes the tint's
// colour for where it is (north red, south blue here), opaque;
// tiles built after it take it too; taken off, every vertex is its own again.
void TestFarPalette::testTintPaintsTheLand()
{
	const Baked b = shipped();
	const Horizon h{ "TF24.dGVycmE", 1, b.content };
	const planet::Atlas atlas = planet::Atlas::resident({ 300, 12, 80, 60, 0.95, 32 });
	Tiles tiles(atlas, planet::Atlas::HOME, h, b.palette, 2);
	tiles.tint(std::make_shared<const Tiles::Tint>([](video::SColor, const Tiling::Vec &at) {
		return at.y >= 0 ? video::SColor(255, 255, 0, 0) : video::SColor(255, 0, 0, 255);
	}));
	const double sea = 300 + h.water_level + 1;
	const Eye eye{ { sea, 0, 0 }, 40, 0, 500, 2 };
	std::vector<Tiles::Drawn> drawn;
	for (int i = 0; i < 600 && (drawn.empty() || i < 100); i++) {
		tiles.update(eye);
		drawn = tiles.drawn();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	UASSERT(!drawn.empty());
	size_t red = 0, blue = 0, other = 0;
	const auto count = [&](bool tinted) {
		red = blue = other = 0;
		for (const Tiles::Drawn &t : drawn)
			for (const auto &part : t.parts)
				for (const far::Vertex &v : part->Vertices->Data) {
					const video::SColor c = v.Color;
					if (c.getRed() == 255 && c.getGreen() == 0 && c.getBlue() == 0)
						red++;
					else if (c.getRed() == 0 && c.getGreen() == 0 && c.getBlue() == 255)
						blue++;
					else
						other++;
				}
		(void)tinted;
	};
	count(true);
	UTEST(other == 0 && red > 0 && blue > 0, "tinted: %zu red, %zu blue, %zu untinted", red, blue, other);
	tiles.tint(nullptr);
	for (int i = 0; i < 600 && tiles.unpainted(); i++) // swept over a few each update
		tiles.update(eye);
	UTEST(tiles.unpainted() == 0, "%zu tiles still in the old tint's colours", tiles.unpainted());
	count(false);
	UTEST(red + blue < other / 100 + 1, "untinted, %zu vertices still red or blue (of %zu)", red + blue, red + blue + other);
}

// Within the eye's cut the near world's blocks are the land (the far view's
// trees stood among the near world's, sprint 42): a tile all inside it is
// only the backdrop for where they haven't arrived (the shader drops it
// nearer than the cut), so it isn't split there once BACKDROP_LEVEL or finer
// (level 0 is finest): a fine tile inside comes only from a parent that
// reached past the cut. One reaching past it is drawn as fine as anywhere.
void TestFarPalette::testInsideTheCutIsFine()
{
	const Baked b = shipped();
	const Horizon h{ "TF24.dGVycmE", 1, b.content };
	const planet::Atlas atlas = planet::Atlas::resident(terragen::Atlas::standard(2000));
	const double sea = 2000 + h.water_level + 1;
	const float CUT = 700;
	auto drawnFor = [&](float cut) {
		Tiles tiles(atlas, planet::Atlas::HOME, h, b.palette, 2);
		const Eye eye{ { sea, 0, 0 }, 10, cut, 1500, 2 };
		std::vector<Tiles::Drawn> drawn;
		settle(tiles, eye);
		UTEST(tiles.pending() == 0, "%zu tiles still being built", tiles.pending());
		// each drawn tile's nearest and farthest, at the sea, its level, and its parent's farthest
		std::vector<std::tuple<double, double, s32, double>> out;
		const Tiling &g = tiles.tiling();
		for (const Tiles::Drawn &t : tiles.drawn()) {
			const double d = std::hypot(t.middle.x * sea / g.radius() - eye.at.x, t.middle.y * sea / g.radius() - eye.at.y,
					t.middle.z * sea / g.radius() - eye.at.z);
			const TileKey up{ t.key.level + 1, t.key.x >> 1, t.key.z >> 1, t.key.face };
			const Tiling::Vec m = g.middle(up);
			const double pd = std::hypot(m.x * sea / g.radius() - eye.at.x, m.y * sea / g.radius() - eye.at.y,
					m.z * sea / g.radius() - eye.at.z);
			out.emplace_back(d - g.diagonal(t.key) / 2, d + g.diagonal(t.key) / 2, t.key.level, pd + g.diagonal(up) / 2);
		}
		return out;
	};
	// Inside the near world's range the far land is as fine as without one:
	// what the near world hasn't loaded shows it, a column at a time
	// (far/mask.h), and it must be the ground's own shape, not coarse cells
	// (it once stopped at cells of 8 blocks there, a checkerboard in every gap).
	const auto whole = drawnFor(0), cut = drawnFor(CUT);
	const auto finestWithin = [&](const auto &tiles) {
		s32 finest = 99;
		size_t n = 0;
		for (const auto &[near, far, level, parent] : tiles)
			if (far + 300 < CUT) {
				finest = std::min(finest, level);
				n++;
			}
		return std::pair{ finest, n };
	};
	const auto [finest_whole, n_whole] = finestWithin(whole);
	const auto [finest_cut, n_cut] = finestWithin(cut);
	UTEST(n_cut > 0, "with a cut, nothing's drawn inside it (%zu tiles): the near world's gaps would be open", cut.size());
	UTEST(finest_cut == finest_whole, "inside the cut the finest tile is level %d, without one %d", finest_cut, finest_whole);
	UTEST(n_cut >= n_whole / 2, "inside the cut %zu tiles, without one %zu: it's coarser there", n_cut, n_whole);
}

