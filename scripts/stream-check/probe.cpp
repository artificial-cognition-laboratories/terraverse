// Standalone terragen: the small streams (Channels' levels below the grid's
// rivers) walked one by one and scored against what a stream should be:
// unbroken from its head to what it joins, always downhill, never over its
// banks, never along a ridge (nor on a dyke over the land round it). Each failure is put down to its cause in
// channels.cpp, so a fix can be measured against the dominant one:
//   scripts/stream-check.sh   (or by hand:)
//   scripts/terragen-tool.sh .output/stream-check/probe scripts/stream-check/probe.cpp
//   .output/stream-check/probe [out dir]
//
// How it walks a stream: each wet node (flow ≥ STREAM_FLOW, as read() draws
// water) is a straight run in the place read() bends to; the probe bends its
// samples back (Read::bent: where each level's channels are measured from), so
// each sample is a column read() itself puts on the stream's line. Everything
// is read in the channels' own place (what World::channelsAt passes read(),
// after the rivers' warp); the game's column is found by undoing that warp,
// and ground() there says whether the game draws water.
//
// A sample is a break where no water's drawn on the stream's line (read()'s
// within half its width, its own or another's it runs beside or meets). Its
// cause, by what read() says there:
//   coarser / finer  another level's valley floor is the land's lowest there
//                    (Read::bed_level): water followed the lowest land (phase 1)
//   same             another of its own level's wet streams owns it
//   sink             its own floor's under land raised over it (fan, pond's sill)
//   dry              a dry channel of its own level owns the lowest land
//   unfound          no column found that read() bends within half its width of
//                    the line: where the bends fold (fold-area%), the probe's
//                    search loses the line; not counted as a break
//   w:river/low/other  read() gives it, but ground() draws no water: a grid
//                    river's bank or floor beside it, ground at the sea's level,
//                    or the rest (karst, a cone, a basin's floor): World's gates
//   sea              it's run into the sea: not a break, not counted
//   karst            it sinks into a limestone platform (World::karst, as
//                    ground() sinks a small stream): a feature, not counted
// (sink and dry are told apart by the land over the bed after read()'s
// last steps, which only bring heights together: a sink by under 2 blocks
// there may be counted as dry.)
#include "terragen/world.h"
#include "terragen/grid.h"
#include "terragen/noise.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <thread>
#include <vector>
using namespace terragen;

namespace {

// channels.cpp's, for rebuilding read()'s bends (its private constants)
// channels.cpp's (private): a stream's flow, and how far its bends reach
// (its lattice over BEND_SHARE), as far as the search for a column goes
const int64_t BEND_SHARE = 12, STREAM_FLOW = 40000;
const uint32_t MEANDER_STREAM = 71; // grid.h's MEANDER: the rivers' warp
// a turn over this many samples (blocks) each way: finer is the bends' pixels
const int TURN_BASE = 3;

struct V
{
	double x = 0, y = 0, z = 0;
	V operator+(const V &o) const { return { x + o.x, y + o.y, z + o.z }; }
	V operator-(const V &o) const { return { x - o.x, y - o.y, z - o.z }; }
	V operator*(double s) const { return { x * s, y * s, z * s }; }
	double dot(const V &o) const { return x * o.x + y * o.y + z * o.z; }
	V cross(const V &o) const { return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x }; }
	double len() const { return std::sqrt(dot(*this)); }
	V unit() const { return *this * (1 / len()); }
};
V vec(const Place &p) { return { (double)p.x / PLACE_ONE, (double)p.y / PLACE_ONE, (double)p.z / PLACE_ONE }; }
Place place(const V &v)
{
	const V u = v.unit();
	return placeOf(std::llround(u.x * PLACE_ONE), std::llround(u.y * PLACE_ONE), std::llround(u.z * PLACE_ONE));
}
double angle(const V &a, const V &b)
{
	const double c = a.dot(b) / std::max(1e-30, a.len() * b.len());
	return std::acos(std::clamp(c, -1.0, 1.0)) * 180 / M_PI;
}
double blocks(int64_t v) { return v / 65536.0; }

enum Cause { OK, COARSER, SAME, FINER, SINK, DRY, LIFTED, W_RIVER, W_LOW, W_OTHER, SEA, KARST, CAUSES };
const char *CAUSE_NAMES[CAUSES] = { "ok", "coarser", "same", "finer", "sink", "dry", "unfound", "w:river", "w:low", "w:other", "sea", "karst" };
// not breaks: where the probe lost the line, and where it's run into the sea
// and where it sinks into karst, a feature of its own (World::karst, as ground() sinks it)
bool counted(int c) { return c != LIFTED && c != SEA && c != KARST; }

struct Region
{
	const char *name;
	int32_t size; // 0: the code's own
	const char *where;
	int span; // blocks across
};
// Fixed, so runs compare: the default world and a planet 100 000 round
// (628 320 a side), each a range, a plain and (the default) a grid river's
// side, where streams come down into it
const char *CODE = "TF22.dGVycmE";
const Region REGIONS[] = {
	{ "default-belt", 0, "belt", 2048 },
	{ "default-plain", 0, "plain", 2048 },
	{ "default-river", 0, "river", 2048 },
	{ "r100k-belt", 628320, "belt", 2048 },
	{ "r100k-plain", 628320, "plain", 2048 },
};

// One stream walked: its samples head to foot
struct Sample
{
	V at;           // the channels' place (unit)
	Cause cause = OK;
	bool wet = false; // ground() draws water here (anyone's)
	double water = NAN, land = NAN;
	fix relief = 0;
	double turn = 0;
	bool folded = false; // read()'s bends fold here: no single column on the line settles
	double far = 0; // how far down its line from its head, in its level's lattices (read()'s REACH_FROM 2, REACH 2.5)
	// across it: falls away both sides (a ridge), one side (a hillside), its water over a bank
	bool ridge = false, hillside = false, perched = false;
	double over = 0; // its water over its lower bank (blocks)
};
struct Walk
{
	int level = 0;
	std::array<int32_t, 3> cell{}, parent{};
	bool sibling = false;
	fix join_t = 0;
	double width = 0; // blocks
	std::vector<Sample> samples;
	// how its water meets what it joins: the angle, and its foot's water against the other's there
	double confluence = NAN, join_step = NAN;
	int uphill = 0;
	double worst_rise = 0;
	int gaps = 0; // runs of break samples
	int jumps = 0; // its columns leaping over 2.5 blocks from one sample to the next
};

class Probe
{
public:
	Probe(const WorldCode &code, const World &world) :
		m_code(code), m_world(world), m_ch(*world.channels()), m_R(World::radiusOf(code) / 65536.0)
	{
		m_meander = (int32_t)std::max<int64_t>(8, ((int64_t)TWO_PI << FRAC) / gridSphere().spacing() >> FRAC);
	}

	double R() const { return m_R; }
	double levelBlocks(int k) const { return (double)m_ch.spacing(k) / PLACE_ONE * m_R; }

	// where read() measures level k's channels from, reading at p (Read::bent)
	Place bent(const Place &p, int k, const Place &) const { return read(p).bent[k]; }
	// the column read() puts at q on level k's channel (q on its straight line):
	// its bends undone, by iteration; and how far off the line it still is (blocks)
	Place unbend(const V &q, int k, const Place &join, const Place *start, double &residual, bool &folded) const
	{
		const V target = q.unit();
		const auto off = [&](const Place &p) { return (vec(bent(p, k, join)) - target).len() * m_R; };
		const auto settle = [&](Place p) {
			for (int it = 0; it < 12; it++) {
				const V step = target - vec(bent(p, k, join));
				p = place(vec(p) + step);
				if (step.len() * m_R < 0.02)
					break;
			}
			return p;
		};
		Place p = settle(start ? *start : place(q));
		residual = off(p);
		folded = residual >= 0.5;
		// where the bends fold (their noise's slope over one), that settles
		// nowhere: search round the last column, and round q less its bend, for
		// one bent within half a block of it, the nearest the last (the stream
		// followed along its fold, not jumped across it)
		if (folded) {
			const V up = target, e = (std::fabs(up.y) < 0.9 ? V{ 0, 1, 0 } : V{ 1, 0, 0 }).cross(up).unit(), s = up.cross(e);
			const V from = start ? vec(*start) : target;
			std::vector<V> centres = { vec(p), from, target - (vec(bent(place(target), k, join)) - target) };
			V best = vec(p);
			double best_d = INFINITY;
			for (const V &c0 : centres) {
				V centre = c0;
				double reach = levelBlocks(k) / BEND_SHARE * 2;
				for (int pass = 0; pass < 3; pass++) {
					const double step = reach / 12;
					V pick = centre;
					double pick_r = INFINITY;
					for (int a = -12; a <= 12; a++)
						for (int b = -12; b <= 12; b++) {
							const V c = centre + (e * (a * step) + s * (b * step)) * (1 / m_R);
							const double r = (vec(bent(place(c), k, join)) - target).len() * m_R;
							if (r < pick_r) {
								pick_r = r;
								pick = c;
							}
						}
					centre = vec(settle(place(pick)));
					reach = step * 2;
				}
				const double r = off(place(centre));
				const double d = r < 0.5 ? (centre.unit() - from.unit()).len() * m_R : 1e9 + r;
				if (d < best_d) {
					best_d = d;
					best = centre;
				}
			}
			p = place(best);
			residual = off(p);
		}
		return p;
	}
	fix range(const Place &p) const
	{
		const fix belt = m_world.between(m_world.grid().tectonics.belt, p);
		return clamp(terragen::div((fix)(belt - ratio(18, 100)), ratio(62, 100)), 0, ONE);
	}
	Channels::Read read(const Place &p) const { return m_ch.read(p, range(p), 0); }
	// the game's column whose channels' place is p (the rivers' warp undone)
	// the rivers' warp at a game column (World::channelsAt's): where it reads the channels
	Place warped(const Place &a) const
	{
		int64_t w[3];
		for (int k = 0; k < 3; k++) {
			const fix f = fbm(a, m_code.seed_hash + MEANDER_STREAM + (uint32_t)k, m_meander / 2, 2, HALF) / 3
					+ fbm(a, m_code.seed_hash + MEANDER_STREAM + 3 + (uint32_t)k, m_meander * 2, 2, HALF) / 16;
			w[k] = (int64_t)f * gridSphere().spacing() >> (2 * FRAC - PLACE_FRAC);
		}
		return placeOf((int64_t)a.x + w[0], (int64_t)a.y + w[1], (int64_t)a.z + w[2]);
	}
	// the game's column whose channels' place is p (the warp undone; damped
	// where plain iteration swung either side of it)
	Place column(const Place &p) const
	{
		V a = vec(p);
		const V target = vec(p);
		double step = 1;
		double last = INFINITY;
		for (int it = 0; it < 80; it++) {
			const V off = target - vec(warped(place(a)));
			const double miss = off.len() * m_R;
			if (miss < 0.01)
				break;
			if (miss > last)
				step = std::max(0.1, step / 2);
			last = miss;
			a = (a + off * step).unit();
		}
		return place(a);
	}

	void walk(Walk &w) const
	{
		const auto [i, j, l] = w.cell;
		const Channels::Node n = m_ch.node(w.level, i, j, l);
		// its way (Channels::path), foot to head
		const Channels::Path way = m_ch.path(w.level, i, j, l);
		const Place join = way.at[0];
		double length = 0;
		for (int q = 0; q < Channels::PATH_PIECES; q++)
			length += (vec(way.at[q + 1]) - vec(way.at[q])).len() * m_R;
		const int steps = std::max(2, (int)std::ceil(length));
		Place last;
		bool have = false;
		for (int s = 0; s <= steps; s++) {
			const double t = 1 - (double)s / steps; // head to foot
			const double on = std::min(t * Channels::PATH_PIECES, Channels::PATH_PIECES - 1e-9);
			const int piece = (int)on;
			const V q = vec(way.at[piece]) + (vec(way.at[piece + 1]) - vec(way.at[piece])) * (on - piece);
			double residual = 0;
			bool folded = false;
			const Place p = unbend(q, w.level, join, have ? &last : nullptr, residual, folded);
			last = p;
			have = true;
			Sample out;
			out.at = vec(p);
			out.far = (1 - t) * length / levelBlocks(w.level);
			out.folded = folded;
			const Channels::Read r = read(p);
			out.land = blocks(r.land);
			out.relief = r.relief;
			const bool drawn = r.water != INT64_MIN && r.water_gap < r.water_width / 2;
			// (its own water, by its level and its line; any water drawn over
			// its line keeps it whole, where streams run side by side or meet)
			const bool own = drawn && (r.pond || (r.water_from == w.level && std::fabs(blocks(r.water_gap) - residual) < 1.0));
			const bool covered = drawn && !own;
			const Ground g = m_world.ground(column(p), m_world.sunlight(column(p), std::nullopt));
			out.wet = g.kind == Water::River || g.kind == Water::Lake;
			if (g.kind == Water::Sea)
				out.cause = SEA; // it's reached the sea (its line runs on over the sea's floor)
			else if (own || covered) {
				if (own)
					out.water = blocks(r.water);
				// the game's own gates (World::ground): a grid river's bank or
				// floor beside it, ground at the sea's level, or karst, a cone, a basin's floor
				out.cause = out.wet ? OK
						: g.river_width > 0 && g.river_dist <= g.river_width / 2 + toFix(6) ? W_RIVER
						: g.terrain <= 1 ? W_LOW
						: w.width < 6 && m_world.karst(column(p), g.column.wet) > HALF ? KARST : W_OTHER;
			} else if (residual >= w.width / 2)
				out.cause = LIFTED;
			else if (r.bed_level != w.level)
				out.cause = r.bed_level < w.level ? COARSER : FINER;
			else if (r.water != INT64_MIN)
				out.cause = SAME;
			else
				out.cause = r.land - r.bed > toFix(1) ? SINK : DRY;
			w.samples.push_back(out);
		}
		// turns, gaps, downhill
		const int N = (int)w.samples.size();
		// (only where the line's found and runs on without a jump: a jump is
		// the line's columns leaping across a fold, counted as such)
		const auto steady = [&](int s) { return w.samples[s].cause != LIFTED && (s == 0 || (w.samples[s].at - w.samples[s - 1].at).len() * m_R < 2.5); };
		for (int s = 1; s < N; s++)
			w.jumps += w.samples[s].cause != LIFTED && w.samples[s - 1].cause != LIFTED && !steady(s);
		for (int s = TURN_BASE; s + TURN_BASE < N; s++) {
			bool ok = true;
			for (int d = -TURN_BASE; d <= TURN_BASE && ok; d++)
				ok = steady(s + d);
			if (!ok)
				continue;
			const V in = w.samples[s].at - w.samples[s - TURN_BASE].at, on = w.samples[s + TURN_BASE].at - w.samples[s].at;
			w.samples[s].turn = std::max(0.01, angle(in, on));
		}
		bool broken = false;
		double lowest = INFINITY;
		for (Sample &s : w.samples) {
			if (!counted(s.cause))
				continue;
			const bool now = s.cause != OK;
			w.gaps += now && !broken;
			broken = now;
			if (!std::isnan(s.water)) {
				if (s.water > lowest + 0.25) {
					w.uphill++;
					w.worst_rise = std::max(w.worst_rise, s.water - lowest);
				}
				lowest = std::min(lowest, s.water);
			}
		}
		if (w.worst_rise > 20 && std::getenv("PROBE_UPHILL")) {
			const Channels::Path pw = m_ch.path(w.level, w.cell[0], w.cell[1], w.cell[2]);
			const Channels::Shape sh = m_ch.shape(w.level, w.cell[0], w.cell[1], w.cell[2]);
			const Channels::Node nd = m_ch.node(w.level, w.cell[0], w.cell[1], w.cell[2]);
			std::string line = "UP L" + std::to_string(w.level) + " sib " + std::to_string(nd.sibling) + " low " + std::to_string(blocks(sh.low)) + " high " + std::to_string(blocks(sh.high)) + " ground";
			for (int64_t gg : pw.ground) line += " " + std::to_string((int)(gg == INT64_MAX ? -1 : blocks(gg)));
			line += " | N " + std::to_string(N) + " rise at";
			double low = INFINITY, worst = 0;
			int at = -1;
			for (int s = 0; s < N; s++)
				if (!std::isnan(w.samples[s].water)) {
					if (w.samples[s].water - low > worst) { worst = w.samples[s].water - low; at = s; }
					low = std::min(low, w.samples[s].water);
				}
			line += " " + std::to_string(at) + ":";
			for (int s = std::max(0, at - 4); s < std::min(N, at + 3); s++)
				line += " " + (std::isnan(w.samples[s].water) ? std::string("-") : std::to_string((int)w.samples[s].water));
			std::printf("%s\n", line.c_str());
		}
		// across it, where its water is: the land at its bank and further out
		for (int s = 0; s < N; s++) {
			Sample &x = w.samples[s];
			if (std::isnan(x.water))
				continue;
			const V along = w.samples[std::min(N - 1, s + 1)].at - w.samples[std::max(0, s - 1)].at;
			if (along.len() == 0)
				continue;
			const V side = x.at.cross(along).unit();
			const double bank = w.width / 2 + 1.5, out = std::max(6.0, w.width * 2);
			// (a bank's dry ground: where the water's drawn, the side's in it, a
			// stream beside or one it meets, and not a bank)
			const auto landAt = [&](double d, bool &dry) {
				const Channels::Read r = read(place(x.at + side * (d / m_R)));
				dry = !(r.water != INT64_MIN && r.water_gap < r.water_width / 2);
				return blocks(r.land);
			};
			bool dl, dr, dummy;
			const double bl = landAt(-bank, dl), br = landAt(bank, dr), ol = landAt(-out, dummy), orr = landAt(out, dummy);
			const double lower = std::min(dl ? bl : INFINITY, dr ? br : INFINITY);
			x.over = std::isinf(lower) ? 0 : x.water - lower;
			x.perched = x.over > 0.5;
			const bool fl = ol < x.land - 1, fr = orr < x.land - 1, rl = ol > x.land + 1, rr = orr > x.land + 1;
			// (and further out, past any bank the stream's corridor raises: a
			// stream on a dyke over the land round it is on a ridge too)
			const double wide = std::max(24.0, w.width * 4);
			const double wl = landAt(-wide, dummy), wr = landAt(wide, dummy);
			x.ridge = (fl && fr) || (wl < x.water - 1 && wr < x.water - 1);
			if (x.ridge && std::getenv("PROBE_DEBUG")) {
				static std::atomic<int> told{ 0 };
				if (told++ < 12) {
					const auto coarse = [&](double d) {
						const Place q = place(x.at + side * (d / m_R));
						return blocks(m_ch.read(q, range(q), m_ch.spacing(w.level - 1)).land);
					};
					const Channels::Read r = read(place(x.at));
					{
						const Channels::Path pw = m_ch.path(w.level, w.cell[0], w.cell[1], w.cell[2]);
						std::printf("DBG ground");
						for (int64_t gg : pw.ground) std::printf(" %.1f", gg == INT64_MAX ? -1.0 : blocks(gg));
						std::printf("\n");
					}
					std::printf("DBG L%d s%d/%d water %.1f land %.1f bed %.1f (lvl %d) coarse %.1f | out24 L %.1f R %.1f | coarse24 L %.1f R %.1f | bank L %.1f R %.1f\n",
							w.level, s, N, x.water, x.land, blocks(r.bed), r.bed_level, coarse(0), wl, wr, coarse(-wide), coarse(wide), bl, br);
				}
			}
			x.hillside = (fl && rr) || (fr && rl);
		}
	}

	const World &world() const { return m_world; }
	const Channels &channels() const { return m_ch; }

private:
	const WorldCode &m_code;
	const World &m_world;
	const Channels &m_ch;
	double m_R;
	int32_t m_meander = 8;
};

int32_t floorBy(int64_t v, int64_t d) { return (int32_t)(v >= 0 ? v / d : -((-v + d - 1) / d)); }

template <typename F>
void parallel(int n, F f)
{
	std::atomic<int> next{ 0 };
	std::vector<std::thread> pool;
	const int T = std::max(1u, std::thread::hardware_concurrency());
	for (int t = 0; t < T; t++)
		pool.emplace_back([&] {
			for (int i; (i = next++) < n;)
				f(i);
		});
	for (std::thread &t : pool)
		t.join();
}

double pct(std::vector<double> v, double p)
{
	if (v.empty())
		return NAN;
	std::sort(v.begin(), v.end());
	return v[std::min(v.size() - 1, (size_t)(v.size() * p))];
}

// A minimal PNG writer (render.cpp's): RGB, deflate "stored" blocks
uint32_t crc(const unsigned char *p, size_t n, uint32_t c = 0xffffffffu)
{
	for (size_t i = 0; i < n; i++) {
		c ^= p[i];
		for (int k = 0; k < 8; k++)
			c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
	}
	return c;
}
void be32(std::vector<unsigned char> &o, uint32_t v) { for (int s = 24; s >= 0; s -= 8) o.push_back((v >> s) & 255); }
void chunk(FILE *f, const char *type, const std::vector<unsigned char> &data)
{
	std::vector<unsigned char> b;
	be32(b, data.size());
	b.insert(b.end(), type, type + 4);
	b.insert(b.end(), data.begin(), data.end());
	be32(b, ~crc(b.data() + 4, b.size() - 4));
	fwrite(b.data(), 1, b.size(), f);
}
void png(const std::string &path, int w, int h, const std::vector<unsigned char> &rgb)
{
	std::vector<unsigned char> raw;
	for (int y = 0; y < h; y++) {
		raw.push_back(0);
		raw.insert(raw.end(), rgb.begin() + (size_t)y * w * 3, rgb.begin() + (size_t)(y + 1) * w * 3);
	}
	std::vector<unsigned char> z = { 0x78, 0x01 };
	for (size_t i = 0; i < raw.size(); i += 65535) {
		size_t n = std::min<size_t>(65535, raw.size() - i);
		z.push_back(i + n == raw.size());
		z.push_back(n & 255); z.push_back(n >> 8); z.push_back(~n & 255); z.push_back((~n >> 8) & 255);
		z.insert(z.end(), raw.begin() + i, raw.begin() + i + n);
	}
	uint32_t a = 1, b = 0;
	for (unsigned char c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
	be32(z, (b << 16) | a);
	std::vector<unsigned char> ihdr;
	be32(ihdr, w); be32(ihdr, h);
	ihdr.insert(ihdr.end(), { 8, 2, 0, 0, 0 });
	FILE *f = fopen(path.c_str(), "wb");
	const unsigned char sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
	fwrite(sig, 1, 8, f);
	chunk(f, "IHDR", ihdr);
	chunk(f, "IDAT", z);
	chunk(f, "IEND", {});
	fclose(f);
}

Dir pick(const Grid &g, const std::string &where)
{
	int32_t best_i = -1;
	double best = -1e18;
	const auto real = [](fix v) { return v / 65536.0; };
	for (int32_t i = 0; i < g.size(); i++) {
		if (g.base[i] < 0)
			continue;
		double score = -1e18;
		if (where == "belt") score = real(g.tectonics.belt[i]);
		else if (where == "plain") score = real(g.tectonics.distance[i]) - 4 * real(g.tectonics.belt[i]) - real(g.base[i]) * 10;
		else if (where == "river") score = g.river(i) && !g.lake[i] && g.base[i] > ratio(2, 100) ? (double)g.drained[i] : -1e18;
		if (score > best) {
			best = score;
			best_i = i;
		}
	}
	return gridSphere().centre(best_i);
}

void region(const Region &reg, const std::string &dir)
{
	WorldCode code = WorldCode::decode(CODE);
	if (reg.size)
		code.size = reg.size;
	World world(code);
	if (!world.channels()) {
		std::printf("%s: no channels\n", reg.name);
		return;
	}
	const Probe probe(code, world);
	const Channels &ch = probe.channels();
	const double R = probe.R();
	const V c = vec(Place(pick(world.grid(), reg.where)));
	const V east = V{ -c.z, 0, c.x }.unit(), south = c.cross(east).unit();
	const int span = reg.span, PX = 2, W = span / PX;
	// a column's offset (blocks) from the centre, and back
	const auto at = [&](double ox, double oy) { return place(c + east * (ox / R) + south * (oy / R)); };
	const auto pixel = [&](const V &p, int &x, int &y) {
		const V d = p.unit() - c;
		x = (int)std::floor((d.dot(east) * R + span / 2.0) / PX);
		y = (int)std::floor((d.dot(south) * R + span / 2.0) / PX);
		return x >= 0 && y >= 0 && x < W && y < W;
	};
	std::printf("\n%s: %s size %d at u,v %.5f,%.5f, %d blocks across (radius %.0f blocks)\n", reg.name, CODE, code.size,
			World::u(Place(pick(world.grid(), reg.where))) / 65536.0, World::v(Place(pick(world.grid(), reg.where))) / 65536.0, span, R);

	// the land and every stream's water, as read() gives them
	std::vector<double> land((size_t)W * W);
	std::vector<uint8_t> water((size_t)W * W);
	parallel(W, [&](int y) {
		for (int x = 0; x < W; x++) {
			const Channels::Read r = probe.read(at((x + 0.5) * PX - span / 2.0, (y + 0.5) * PX - span / 2.0));
			land[(size_t)y * W + x] = blocks(r.land);
			water[(size_t)y * W + x] = r.water != INT64_MIN && r.water_gap < std::max<int64_t>(r.water_width / 2, toFix(PX) / 2);
		}
	});

	// the wet nodes whose heads are in the region, each level's
	std::vector<Walk> walks;
	for (int k = 1; k <= ch.levels(); k++) {
		const int64_t L = ch.spacing(k);
		const double Lb = probe.levelBlocks(k);
		std::vector<std::array<int32_t, 3>> cells;
		const double stride = std::max(1.0, Lb / 3);
		for (double oy = -span / 2.0; oy <= span / 2.0; oy += stride)
			for (double ox = -span / 2.0; ox <= span / 2.0; ox += stride) {
				const Place p = at(ox, oy);
				const int32_t i = floorBy(p.x, L), j = floorBy(p.y, L), l = floorBy(p.z, L);
				for (int a = -1; a <= 1; a++)
					for (int b = -1; b <= 1; b++)
						for (int d = -1; d <= 1; d++)
							cells.push_back({ i + a, j + b, l + d });
			}
		std::sort(cells.begin(), cells.end());
		cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
		std::vector<int> keep(cells.size(), 0);
		std::vector<Walk> found(cells.size());
		parallel((int)cells.size(), [&](int n) {
			const auto [i, j, l] = cells[n];
			const Channels::Node node = ch.node(k, i, j, l); // (a copy: its shape asks for more)
			if (!node.exists)
				return;
			if (std::getenv("PROBE_WET")) {
				static std::atomic<int> told{ 0 };
				const Channels::Shape sh = ch.shape(k, i, j, l);
				if (told++ < 40)
					std::printf("WET L%d sib %d wet %d rain %.2f area %lld flow %lld\n", k, node.sibling, node.wet, node.rain / 65536.0, (long long)sh.area, (long long)(sh.area * node.rain >> 16));
			}
			const Channels::Shape &sh = ch.shape(k, i, j, l);
			const int64_t flow = mulShift(sh.area, node.rain, FRAC);
			int x, y;
			if (flow < STREAM_FLOW || !pixel(vec(node.at), x, y))
				return;
			Walk &w = found[n];
			w.level = k;
			w.cell = cells[n];
			w.sibling = node.sibling;
			w.parent = node.sibling ? node.into : node.parent;
			w.join_t = node.t;
			w.width = blocks(ch.streamWidth(flow));
			keep[n] = 1;
		});
		for (size_t n = 0; n < cells.size(); n++)
			if (keep[n])
				walks.push_back(found[n]);
	}
	if (walks.empty()) {
		std::printf("  no wet streams\n");
		return;
	}
	parallel((int)walks.size(), [&](int n) { probe.walk(walks[n]); });
	// joins: the angle its water meets what it joins at, and the step between their waters
	std::map<std::array<int32_t, 4>, int> index;
	for (size_t n = 0; n < walks.size(); n++)
		index[{ walks[n].level, walks[n].cell[0], walks[n].cell[1], walks[n].cell[2] }] = (int)n;
	for (Walk &w : walks) {
		const int into_level = w.sibling ? w.level : w.level - 1;
		const auto it = index.find({ into_level, w.parent[0], w.parent[1], w.parent[2] });
		if (into_level < 1 || it == index.end() || w.samples.size() < 2 * TURN_BASE + 2)
			continue;
		const Walk &o = walks[it->second];
		// its sample nearest the join: t is from its foot (sample count − 1)
		const int m = (int)o.samples.size() - 1;
		const int s = std::clamp((int)std::lround((1 - w.join_t / 65536.0) * m), TURN_BASE, std::max(TURN_BASE, m - TURN_BASE));
		if (s + TURN_BASE > m)
			continue;
		const int e = (int)w.samples.size() - 1;
		const V in = w.samples[e].at - w.samples[e - 2 * TURN_BASE].at;
		const V on = o.samples[s + TURN_BASE].at - o.samples[s - TURN_BASE].at;
		w.confluence = angle(in, on);
		double foot = NAN, there = NAN;
		for (int q = e; q >= std::max(0, e - 3) && std::isnan(foot); q--)
			foot = w.samples[q].water;
		for (int q = std::max(0, s - 3); q <= std::min(m, s + 3) && std::isnan(there); q++)
			there = o.samples[q].water;
		w.join_step = foot - there;
	}

	// where each level's bends fold: the share of the region whose columns
	// read() bends with a Jacobian turned over (det < 0), so a straight line
	// comes out in loops and pieces (its own bend in full, its foot far)
	std::array<double, Channels::MOST_LEVELS + 1> fold_area{};
	for (int k = 1; k <= ch.levels(); k++) {
		const Place far = at(1e7, 0);
		std::vector<int> turned(64, 0);
		parallel(64, [&](int y) {
			for (int x = 0; x < 64; x++) {
				const double ox = (x + 0.5) * span / 64 - span / 2.0, oy = (y + 0.5) * span / 64 - span / 2.0, h = 0.25;
				const V f = vec(probe.bent(at(ox, oy), k, far)), fx = vec(probe.bent(at(ox + h, oy), k, far)), fy = vec(probe.bent(at(ox, oy + h), k, far));
				const double a = (fx - f).dot(east), b = (fy - f).dot(east), c2 = (fx - f).dot(south), d = (fy - f).dot(south);
				turned[y] += a * d - b * c2 < 0;
			}
		});
		for (int v : turned)
			fold_area[k] += v * 100.0 / (64 * 64);
	}

	// the summary, a level a row
	std::printf("%-4s %6s %5s %7s %6s %6s %5s |", "lvl", "blocks", "strms", "samples", "break%", "dry%", "gaps");
	for (int c = 1; c < CAUSES; c++)
		std::printf(" %7s", CAUSE_NAMES[c]);
	std::printf(" | turn p50/p95/max  >45 >90 | conf p50 >90 | uphill worst jstep<-1 | ridge%% hill%% perch%% rel>.5%% | >2L%% brk<2L brk>2L | unfnd%% jumps fold-area%% | over p50/p95\n");
	for (int k = 0; k <= ch.levels(); k++) {
		long samples = 0, causes[CAUSES] = {}, dry = 0, gaps = 0, n = 0, t45 = 0, t90 = 0, c90 = 0, uphill = 0, jdown = 0;
		long drawn = 0, ridge = 0, hill = 0, perched = 0, relief = 0, past = 0, past_broken = 0, near_broken = 0, folds = 0, jumps = 0;
		double worst = 0;
		std::vector<double> turns, confl, overs;
		for (const Walk &w : walks) {
			if (k != 0 && w.level != k)
				continue;
			n++;
			gaps += w.gaps;
			jumps += w.jumps;
			uphill += w.uphill;
			worst = std::max(worst, w.worst_rise);
			if (!std::isnan(w.confluence)) {
				confl.push_back(w.confluence);
				c90 += w.confluence > 90;
			}
			jdown += w.join_step < -1;
			for (const Sample &s : w.samples) {
				samples++;
				causes[s.cause]++;
				past += s.far >= 2;
				folds += s.folded;
				(s.far >= 2 ? past_broken : near_broken) += counted(s.cause) && s.cause != OK;
				dry += !s.wet;
				if (s.turn > 0) {
					turns.push_back(s.turn);
					t45 += s.turn > 45;
					t90 += s.turn > 90;
				}
				if (!std::isnan(s.water)) {
					drawn++;
					ridge += s.ridge;
					hill += s.hillside;
					perched += s.perched;
					overs.push_back(std::max(0.0, s.over));
					relief += s.relief > HALF;
				}
			}
		}
		if (n == 0)
			continue;
		const double S = std::max(1L, samples) / 100.0, D = std::max(1L, drawn) / 100.0;
		char lvl[8];
		std::snprintf(lvl, sizeof lvl, "%s", k == 0 ? "all" : std::to_string(k).c_str());
		std::printf("%-4s %6.0f %5ld %7ld %6.1f %6.1f %5.2f |", lvl, k == 0 ? 0.0 : probe.levelBlocks(k), n, samples,
				(samples - causes[OK] - causes[LIFTED] - causes[SEA] - causes[KARST]) * 100.0 / std::max(1L, samples - causes[LIFTED] - causes[SEA] - causes[KARST]), dry / S, (double)gaps / n);
		for (int c = 1; c < CAUSES; c++)
			std::printf(" %7.1f", causes[c] / S);
		std::printf(" | %4.0f %4.0f %4.0f %5ld %3ld | %4.0f %5ld | %6ld %5.1f %8ld | %6.1f %5.1f %6.1f %6.1f | %4.1f %6.1f %6.1f | %5.1f %5ld %5.1f | %4.1f %5.1f\n", pct(turns, 0.5), pct(turns, 0.95),
				pct(turns, 1), t45, t90, pct(confl, 0.5), c90, uphill, worst, jdown, ridge / D, hill / D, perched / D, relief / D,
				past / S, 100.0 * near_broken / std::max(1L, samples - past), 100.0 * past_broken / std::max(1L, past), causes[LIFTED] / S, jumps, k == 0 ? NAN : fold_area[k], pct(overs, 0.5), pct(overs, 0.95));
	}

	// examples: the longest run of each failure, its middle (u,v of the game's column)
	{
		struct Ex { double score = 0; V at; int level = 0; };
		std::map<std::string, Ex> ex;
		const auto note = [&](const std::string &what, double score, const V &p, int level) {
			Ex &e = ex[what];
			if (score > e.score)
				e = { score, p, level };
		};
		for (const Walk &w : walks) {
			int run = 0;
			for (size_t s = 0; s < w.samples.size(); s++) {
				const Sample &x = w.samples[s];
				run = x.cause != OK ? run + 1 : 0;
				if (x.cause != OK && (s + 1 == w.samples.size() || w.samples[s + 1].cause == OK))
					note(std::string("break:") + CAUSE_NAMES[x.cause], run, w.samples[s - run / 2].at, w.level);
				note("sharp", x.turn, x.at, w.level);
				if (x.ridge) note("ridge", 1 + x.relief / 65536.0, x.at, w.level);
				if (x.perched) note("perched", x.over, x.at, w.level);
			}
			if (w.worst_rise > 0)
				for (const Sample &x : w.samples)
					if (!std::isnan(x.water)) { note("uphill", w.worst_rise, x.at, w.level); break; }
		}
		for (const auto &[what, e] : ex) {
			const Place g = probe.column(place(e.at));
			std::printf("  e.g. %-14s level %d at u,v %.6f,%.6f (%.0f)\n", what.c_str(), e.level, World::u(g) / 65536.0, World::v(g) / 65536.0, e.score);
		}
	}

	// the picture: hillshade, read()'s water blue; on the streams walked, red a
	// break, yellow a sharp turn (>45°), magenta uphill, a ridge or over a bank
	std::vector<unsigned char> rgb((size_t)W * W * 3);
	for (int y = 0; y < W; y++)
		for (int x = 0; x < W; x++) {
			const size_t o = (size_t)y * W + x;
			const double h = land[o], hx = land[(size_t)y * W + std::max(0, x - 1)], hy = land[(size_t)std::max(0, y - 1) * W + x];
			const double shade = std::clamp(0.75 + ((h - hx) + (h - hy)) / PX * 0.35, 0.2, 1.3);
			double r = 150 * shade, g = 140 * shade, b = 120 * shade;
			if (h <= 0) { r = 40; g = 60; b = 120; }
			if (water[o]) { r = 40; g = 110; b = 230; }
			rgb[o * 3] = (unsigned char)std::clamp(r, 0.0, 255.0);
			rgb[o * 3 + 1] = (unsigned char)std::clamp(g, 0.0, 255.0);
			rgb[o * 3 + 2] = (unsigned char)std::clamp(b, 0.0, 255.0);
		}
	const auto paint = [&](const V &p, unsigned char r, unsigned char g, unsigned char b) {
		int x, y;
		if (!pixel(p, x, y))
			return;
		const size_t o = ((size_t)y * W + x) * 3;
		rgb[o] = r; rgb[o + 1] = g; rgb[o + 2] = b;
	};
	for (const Walk &w : walks)
		for (const Sample &s : w.samples)
			if (s.cause != OK) paint(s.at, 230, 30, 30);
	for (const Walk &w : walks)
		for (const Sample &s : w.samples) {
			if (s.ridge || s.perched) paint(s.at, 230, 40, 230);
			if (s.turn > 45) paint(s.at, 250, 230, 30);
		}
	for (const Walk &w : walks)
		if (w.worst_rise > 0)
			for (const Sample &s : w.samples)
				if (!std::isnan(s.water)) paint(s.at, 230, 40, 230);
	const std::string path = dir + "/" + reg.name + ".png";
	png(path, W, W, rgb);
	std::printf("  image %s (%d px, %d blocks a pixel)\n", path.c_str(), W, PX);
}

} // namespace

int main(int argc, char **argv)
{
	const std::string dir = argc > 1 ? argv[1] : ".output/stream-check";
	const auto t0 = std::chrono::steady_clock::now();
	std::printf("stream probe · break%%: samples on a stream's line without its own water, by cause (%% of samples); dry%%: ground() draws no water\n"
				"turn: degrees over %d blocks each way · conf: the angle a stream meets what it joins · jstep<-1: its foot's water a block under what it joins\n"
				"ridge/hill/perch: where it has water, the land falls away both sides / one side, its water over a bank · rel>.5: Read::relief over 0.5\n",
			TURN_BASE);
	// (a region by name, the rest left out: argv[2])
	for (const Region &r : REGIONS)
		if (argc <= 2 || r.name == std::string(argv[2]))
			region(r, dir);
	std::fprintf(stderr, "stream probe: %.0f s\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
}
