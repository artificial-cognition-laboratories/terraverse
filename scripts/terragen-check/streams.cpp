// Prototype (sprint 116, phase 6b): the land below the grid built from its
// streams, after Dendry (Gaillard, Peytavie, Galin et al. 2019), from the
// paper, not its code. Floats, on a flat patch: to judge the method by eye.
//   scripts/terragen-tool.sh /tmp/streams scripts/terragen-check/streams.cpp
//   [SIZE=628320] /tmp/streams <code> out.png [u,v | belt | river | peak] [span km] [pixels]
//
// The tree: level 0 is the grid's own (every cell to its receiver, its bed the
// grid's water surface). Each finer level hashes one point per lattice cell,
// half as wide as the last, and joins it to the best point on any coarser
// branch near it: near, down the land's slope, and never above it. Its bed is
// the join's plus stream power's fall at its balance (S = k_s · cell / L), held
// under the land, so every branch falls all the way to the sea. Each branch is
// bent by midpoint displacement within its own cell.
// The land: from each level's nearest branch, its bed plus a valley side
// rising at that level's hillside gradient; the land is the lowest of them,
// capped a little over the grid's surface. Ridges are what's left between.
// Water: where a wet branch's channel runs, at its bed.
#include "terragen/world.h"
#include "terragen/sphere.h"
#include "png.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
using namespace terragen;

namespace {

double real(fix v) { return v / (double)ONE; }
double env(const char *k, double d) { const char *v = std::getenv(k); return v ? std::stod(v) : d; }

struct V2 { double x, y; };
V2 operator-(V2 a, V2 b) { return { a.x - b.x, a.y - b.y }; }
V2 operator+(V2 a, V2 b) { return { a.x + b.x, a.y + b.y }; }
V2 operator*(V2 a, double s) { return { a.x * s, a.y * s }; }
double dot(V2 a, V2 b) { return a.x * b.x + a.y * b.y; }
double len(V2 a) { return std::hypot(a.x, a.y); }

double hash(int64_t i, int64_t j, uint64_t k)
{
	uint64_t v = (uint64_t)i * 0x9e3779b97f4a7c15ull ^ (uint64_t)j * 0xc2b2ae3d27d4eb4full ^ (k + 1) * 0x165667b19e3779f9ull;
	v ^= v >> 29; v *= 0xbf58476d1ce4e5b9ull; v ^= v >> 32; v *= 0x94d049bb133111ebull; v ^= v >> 31;
	return (double)(v >> 11) / (double)(1ull << 53);
}

// A branch: a bent polyline from its head down to where it joins, its bed
// falling along it, and its channel's width (0: a dry valley)
struct Branch {
	std::vector<V2> pts;
	std::vector<double> bed, width;
	int level;
};

struct Near { double d = 1e18, bed = 0, width = 0; V2 q{ 0, 0 }; const Branch *b = nullptr; };

Near nearest(const Branch &b, V2 p)
{
	Near n;
	for (size_t i = 0; i + 1 < b.pts.size(); i++) {
		const V2 a = b.pts[i], e = b.pts[i + 1] - a;
		const double l2 = dot(e, e);
		const double t = l2 > 0 ? std::clamp(dot(p - a, e) / l2, 0.0, 1.0) : 0;
		const double d = len(p - (a + e * t));
		if (d < n.d) {
			n.d = d; n.q = a + e * t; n.b = &b;
			n.bed = b.bed[i] + (b.bed[i + 1] - b.bed[i]) * t;
			n.width = b.width[i] + (b.width[i + 1] - b.width[i]) * t;
		}
	}
	return n;
}

// Bend a run through its control points by midpoint displacement, DEPTH
// times, each offset across a piece a share of its length, then round it
// (Chaikin, twice). Bed and width are carried as averages, so a bed that falls
// through the control points falls all along.
void bend(Branch &b, std::vector<V2> pts, std::vector<double> bed, std::vector<double> width, int depth, uint64_t key, int64_t i, int64_t j)
{
	const double SHARE = env("BEND", 0.15);
	for (int d = 0; d < depth; d++) {
		std::vector<V2> np;
		std::vector<double> nb, nw;
		for (size_t s = 0; s + 1 < pts.size(); s++) {
			const V2 a = pts[s], c = pts[s + 1], e = c - a;
			const double l = len(e);
			const V2 across = l > 0 ? V2{ -e.y / l, e.x / l } : V2{ 0, 0 };
			const double off = (hash(i, j, key * 4096 + d * 512 + s) * 2 - 1) * SHARE * l;
			np.push_back(a); nb.push_back(bed[s]); nw.push_back(width[s]);
			np.push_back((a + c) * 0.5 + across * off); nb.push_back((bed[s] + bed[s + 1]) / 2); nw.push_back((width[s] + width[s + 1]) / 2);
		}
		np.push_back(pts.back()); nb.push_back(bed.back()); nw.push_back(width.back());
		pts = np; bed = nb; width = nw;
	}
	for (int c = 0; c < 2; c++) {
		std::vector<V2> np = { pts.front() };
		std::vector<double> nb = { bed.front() }, nw = { width.front() };
		for (size_t s = 0; s + 1 < pts.size(); s++) {
			np.push_back(pts[s] * 0.75 + pts[s + 1] * 0.25); nb.push_back(bed[s] * 0.75 + bed[s + 1] * 0.25); nw.push_back(width[s] * 0.75 + width[s + 1] * 0.25);
			np.push_back(pts[s] * 0.25 + pts[s + 1] * 0.75); nb.push_back(bed[s] * 0.25 + bed[s + 1] * 0.75); nw.push_back(width[s] * 0.25 + width[s + 1] * 0.75);
		}
		np.push_back(pts.back()); nb.push_back(bed.back()); nw.push_back(width.back());
		pts = np; bed = nb; width = nw;
	}
	b.pts = pts; b.bed = bed; b.width = width;
}

// Whether the run a→b crosses any piece of a branch (touching at b allowed)
bool crosses(V2 a, V2 b, const Branch &br)
{
	const auto cr = [](V2 o, V2 p, V2 q) { return (p.x - o.x) * (q.y - o.y) - (p.y - o.y) * (q.x - o.x); };
	for (size_t i = 0; i + 1 < br.pts.size(); i++) {
		const V2 c = br.pts[i], d = br.pts[i + 1];
		const double d1 = cr(c, d, a), d2 = cr(c, d, b), d3 = cr(a, b, c), d4 = cr(a, b, d);
		if (((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0)) && std::abs(d2) > 1e-6 && std::abs(d3) > 1e-6 && std::abs(d4) > 1e-6)
			return true;
	}
	return false;
}

// A field sampled bilinearly from a lattice
struct Field {
	std::vector<double> v;
	int n;
	double lo, step;
	double at(V2 p) const
	{
		const double fx = std::clamp((p.x - lo) / step, 0.0, n - 1.001), fy = std::clamp((p.y - lo) / step, 0.0, n - 1.001);
		const int x = (int)fx, y = (int)fy;
		const double tx = fx - x, ty = fy - y;
		const auto g = [&](int xx, int yy) { return v[(size_t)yy * n + xx]; };
		return (g(x, y) * (1 - tx) + g(x + 1, y) * tx) * (1 - ty) + (g(x, y + 1) * (1 - tx) + g(x + 1, y + 1) * tx) * ty;
	}
	V2 down(V2 p) const
	{
		const double e = step;
		const V2 g = { (at({ p.x + e, p.y }) - at({ p.x - e, p.y })) / (2 * e), (at({ p.x, p.y + e }) - at({ p.x, p.y - e })) / (2 * e) };
		const double l = len(g);
		return l > 1e-9 ? g * (-1 / l) : V2{ 0, 0 };
	}
};

// A level's branches, bucketed by every lattice cell they pass through
struct Level {
	double L;
	std::vector<Branch> branches;
	std::unordered_map<int64_t, std::vector<int>> buckets;
	static int64_t key(int64_t i, int64_t j) { return i * 1000003 + j; }
	void add(Branch b)
	{
		const int id = (int)branches.size();
		for (const V2 &p : b.pts) {
			std::vector<int> &in = buckets[key((int64_t)std::floor(p.x / L), (int64_t)std::floor(p.y / L))];
			if (in.empty() || in.back() != id)
				in.push_back(id);
		}
		branches.push_back(std::move(b));
	}
	template <class F> void around(V2 p, int reach, F f) const
	{
		const int64_t ci = (int64_t)std::floor(p.x / L), cj = (int64_t)std::floor(p.y / L);
		std::vector<int> seen;
		for (int64_t j = cj - reach; j <= cj + reach; j++)
			for (int64_t i = ci - reach; i <= ci + reach; i++) {
				const auto it = buckets.find(key(i, j));
				if (it != buckets.end())
					seen.insert(seen.end(), it->second.begin(), it->second.end());
			}
		std::sort(seen.begin(), seen.end());
		seen.erase(std::unique(seen.begin(), seen.end()), seen.end());
		for (int b : seen)
			f(branches[b]);
	}
};

} // namespace

int main(int argc, char **argv)
{
	if (argc < 3) {
		std::fprintf(stderr, "usage: streams <code> <out.png> [u,v|belt|river|peak] [span km] [pixels]\n");
		return 2;
	}
	WorldCode code = WorldCode::decode(argv[1]);
	if (const char *size = std::getenv("SIZE"))
		code.size = std::stoi(size);
	const std::string where = argc > 3 ? argv[3] : "belt";
	const double km = argc > 4 ? std::stod(argv[4]) : 8;
	const int N = argc > 5 ? std::stoi(argv[5]) : 1024;
	std::shared_ptr<const Grid> g = gridFor(code);
	World world(code);
	const Sphere &sphere = gridSphere();
	const double R = World::radiusOf(code) / 65536.0;
	const double perM = real(blocksPerMetre(code));
	const double cell = World::cellBlocksOf(code) / 65536.0;
	const double hs = real(heightScale(code));

	// where: a spot by u,v, or the grid cell that best fits a kind
	double u0, v0;
	if (where.find(',') != std::string::npos) {
		u0 = std::stod(where.substr(0, where.find(',')));
		v0 = std::stod(where.substr(where.find(',') + 1));
	} else {
		int32_t pick = 0;
		double best = -1e18;
		for (int32_t i = 0; i < g->size(); i++) {
			if (g->base[i] < 0)
				continue;
			double s = -1e18;
			if (where == "belt") s = real(g->tectonics.belt[i]);
			else if (where == "peak") s = real(g->base[i]);
			else if (where == "river") s = g->river(i) && !g->lake[i] && g->base[i] > ratio(2, 100) ? (double)g->drained[i] : -1e18;
			if (s > best) { best = s; pick = i; }
		}
		u0 = real(World::u(sphere.centre(pick)));
		v0 = real(World::v(sphere.centre(pick)));
	}
	const double lat = (0.5 - v0) * M_PI, coslat = std::max(0.05, std::cos(lat));
	// the patch's flat frame: x east, y south, blocks from its middle
	const auto place = [&](V2 p) {
		const double u = u0 + p.x / (2 * M_PI * R * coslat), v = std::clamp(v0 + p.y / (M_PI * R), 0.0, 1.0);
		const double la = (0.5 - v) * M_PI, lo = (u - 0.5) * 2 * M_PI;
		return Place((int32_t)std::llround(std::cos(la) * std::cos(lo) * PLACE_ONE), (int32_t)std::llround(std::sin(la) * PLACE_ONE),
				(int32_t)std::llround(std::cos(la) * std::sin(lo) * PLACE_ONE));
	};
	const auto local = [&](const Dir &d) {
		double du = real(World::u(d)) - u0;
		du -= std::round(du);
		return V2{ du * 2 * M_PI * R * coslat, (real(World::v(d)) - v0) * M_PI * R };
	};
	const double span = km * 1000 * perM, half = span / 2;
	std::printf("region %s at %.4f,%.4f · %.1f km (%.0f blocks) · cell %.0f blocks · %.2f blocks a metre\n", where.c_str(), u0, v0, km, span, cell, perM);

	// the grid's smooth surface and channel steepness, over the patch and a margin
	const double margin = 3 * cell;
	const double fstep = std::max(8.0, cell / 10);
	const int FN = (int)std::ceil(2 * (half + margin) / fstep) + 1;
	Field base{ std::vector<double>((size_t)FN * FN), FN, -(half + margin), fstep };
	Field ks = base, rain = base;
	for (int y = 0; y < FN; y++)
		for (int x = 0; x < FN; x++) {
			const Place at = place({ base.lo + x * fstep, base.lo + y * fstep });
			const size_t k = (size_t)y * FN + x;
			base.v[k] = world.gridHeight(at) / 65536.0;
			const int32_t c = world.cell(at.dir()), r = g->receiver[c];
			double gap = 1;
			const fix *d = sphere.distancesBegin(c);
			for (const int32_t *p = sphere.neighboursBegin(c); p != sphere.neighboursEnd(c); p++, d++)
				if (*p == r) gap = *d / (double)sphere.spacing();
			const double fall = r < 0 ? 0 : std::max(0.0, real(g->filled[c] - std::max<fix>(0, g->filled[r]))) * hs;
			ks.v[k] = fall / (gap * cell) * std::sqrt(std::max(1.0, real((fix)std::min<int64_t>(g->drained[c], INT32_MAX))));
			rain.v[k] = real(g->rain(c));
		}
	// smooth the grid's facets: three box passes, a quarter cell wide
	const auto blur = [&](Field &f) {
		const int r = std::max(1, (int)(cell / 4 / fstep));
		for (int pass = 0; pass < 3; pass++)
			for (int axis = 0; axis < 2; axis++) {
				const std::vector<double> was = f.v;
				for (int y = 0; y < FN; y++)
					for (int x = 0; x < FN; x++) {
						double s = 0;
						for (int o = -r; o <= r; o++)
							s += was[(size_t)(axis ? std::clamp(y + o, 0, FN - 1) : y) * FN + (axis ? x : std::clamp(x + o, 0, FN - 1))];
						f.v[(size_t)y * FN + x] = s / (2 * r + 1);
					}
			}
	};
	blur(base);
	blur(ks);

	const double RELIEF = env("RELIEF", 0.3), FALL = env("FALL", 0.35), CRITICAL = env("CRITICAL", 0.5);
	const int STEPS = (int)env("STEPS", 16);
	const double WIDTH = env("WIDTH", 3.0), WET = env("WET", 0.004), TOWARD = env("TOWARD", 0.6), MINFALL = env("MINFALL", 0.002);
	// a valley's depth at a level's scale, as stream power at its balance cuts
	const auto depth = [&](V2 p, double L) { return RELIEF * std::max(0.0, ks.at(p)) * cell * std::pow(L / (cell / 2), FALL); };
	const auto side = [&](V2 p, double L) { return std::clamp(4 * depth(p, L) / L, 0.01, CRITICAL); };
	const auto widthOf = [&](double cells) { return std::max(1.0, WIDTH * std::sqrt(cells)); }; // hydraulic geometry, w ∝ Q^½

	const auto started = std::chrono::steady_clock::now();
	const int LEVELS = (int)env("LEVELS", 5);
	const double step = span / N;
	const double CAP = env("CAP", 0.5), VALLEY = env("VALLEY", 0.5);
	// a valley side's profile, 0 at the channel to 1 at the land above: PROFILE 0
	// rounds its crest (t(2 − t), steepest at the foot), 1 a smoothstep, 2 straight
	const int PROFILE = (int)env("PROFILE", 0);
	const double PROFILE_SLOPE = PROFILE == 0 ? 2 : PROFILE == 1 ? 1.5 : 1; // its steepest, × cut / R
	const auto profile = [&](double t) { return PROFILE == 0 ? t * (2 - t) : PROFILE == 1 ? t * t * (3 - 2 * t) : t; };
	std::vector<Level> levels;
	// The land at p carved by the levels below `upto`: from the grid's surface
	// (a little over it), each level's branches near enough cut their valleys
	// into what the coarser left: a floor the channel wide, sides rising to
	// that land over a half-width (wider where it's deep, so never past a
	// hillside's gradient); the land is the lowest of them, so ridges stand
	// where valleys meet. `wet`: the widest channel within `pixel`, if asked.
	const auto carve = [&](V2 p, size_t upto, Near *wet, double pixel) {
		double land = base.at(p) + CAP * depth(p, cell / 2);
		for (size_t m = 0; m < upto && m < levels.size(); m++) {
			const Level &l = levels[m];
			const double above = land;
			l.around(p, m == 0 ? 2 : 3, [&](const Branch &b) {
				const Near n = nearest(b, p);
				if (wet && n.width > 0 && n.d < std::max(n.width / 2, pixel) && (!wet->b || n.width > wet->width))
					*wet = n;
				if (n.bed >= above)
					return;
				const double cut = above - n.bed, R = std::max(VALLEY * l.L, PROFILE_SLOPE * cut / CRITICAL);
				const double t = std::clamp((n.d - n.width / 2) / R, 0.0, 1.0);
				land = std::min(land, n.bed + cut * profile(t));
			});
		}
		return land;
	};
	// level 0: the grid's own tree, as main stems: each cell carries on down
	// the line of its biggest donor, so a river is one run across many cells;
	// a stem's foot joins its receiver's stem where it's nearest, trunks first
	{
		Level l0{ cell };
		const auto in = [&](int32_t c) {
			const V2 a = local(sphere.centre(c));
			return g->base[c] >= 0 && g->receiver[c] >= 0 && std::abs(a.x) < half + margin && std::abs(a.y) < half + margin;
		};
		std::vector<int32_t> main(g->size(), -1);
		for (int32_t c = 0; c < g->size(); c++) {
			const int32_t r = g->receiver[c];
			if (r >= 0 && g->base[c] >= 0 && (main[r] < 0 || g->drained[c] > g->drained[main[r]]))
				main[r] = c;
		}
		std::vector<std::vector<int32_t>> stems;
		for (int32_t c = 0; c < g->size(); c++) {
			if (!in(c) || (main[c] >= 0 && in(main[c])))
				continue; // not a head
			std::vector<int32_t> stem = { c };
			while (in(g->receiver[stem.back()]) && main[g->receiver[stem.back()]] == stem.back())
				stem.push_back(g->receiver[stem.back()]);
			stems.push_back(stem);
		}
		if (std::getenv("DEBUG"))
			for (const auto &stem : stems)
				if (g->drained[stem.back()] > g->river_threshold) {
					std::printf("stem %zu cells:", stem.size());
					for (int32_t c : stem) { const V2 a = local(sphere.centre(c)); std::printf(" (%.0f,%.0f f%.1f s%d l%d)", a.x, a.y, real(g->filled[c]) * hs, g->stream(c), g->lake[c]); }
					std::printf(" → %d\n", g->receiver[stem.back()]);
				}
		std::sort(stems.begin(), stems.end(), [&](const auto &a, const auto &b) { return g->drained[a.back()] > g->drained[b.back()]; });
		std::vector<int> stemOf(g->size(), -1);
		for (const auto &stem : stems) {
			std::vector<V2> pts;
			std::vector<double> bed, width;
			for (int32_t c : stem) {
				pts.push_back(local(sphere.centre(c)));
				bed.push_back(std::min(bed.empty() ? 1e18 : bed.back(), real(g->filled[c]) * hs));
				{
					const double cells = real((fix)std::min<int64_t>(g->drained[c], INT32_MAX)); // (rain-weighted)
					width.push_back(cells >= WET ? widthOf(cells) : 0);
				}
			}
			const int32_t r = g->receiver[stem.back()];
			if (stemOf[r] >= 0) { // onto its receiver's stem
				const Near n = nearest(l0.branches[stemOf[r]], pts.back());
				pts.push_back(n.q); bed.push_back(std::min(bed.back(), n.bed)); width.push_back(width.back());
			} else {
				pts.push_back(local(sphere.centre(r))); bed.push_back(std::min(bed.back(), std::max(0.0, real(g->filled[r]) * hs))); width.push_back(width.back());
			}
			Branch br;
			br.level = 0;
			bend(br, pts, bed, width, 4, 0, stem.front(), 0);
			for (int32_t c : stem)
				stemOf[c] = (int)l0.branches.size();
			l0.add(std::move(br));
		}
		levels.push_back(std::move(l0));
	}
	// finer levels: a point a lattice cell, joined to the best branch near it
	int points = 0, forced = 0;
	for (int k = 1; k <= LEVELS; k++) {
		const double L = cell / std::pow(2, k);
		Level lk{ L };
		const double reach = half + 3 * L;
		for (int64_t j = (int64_t)std::floor(-reach / L); j <= (int64_t)std::ceil(reach / L); j++)
			for (int64_t i = (int64_t)std::floor(-reach / L); i <= (int64_t)std::ceil(reach / L); i++) {
				const V2 p = { (i + 0.15 + 0.7 * hash(i, j, k * 2)) * L, (j + 0.15 + 0.7 * hash(i, j, k * 2 + 1)) * L };
				if (base.at(p) <= 0)
					continue;
				// walk down the land the coarser levels left, a quarter of the
				// lattice at a step, till a coarser channel takes it
				const double cut = depth(p, L);
				const auto joinAt = [&](V2 q) {
					Near best;
					for (int m = 0; m < k; m++)
						levels[m].around(q, m == 0 ? 2 : 3, [&](const Branch &b) {
							const Near n = nearest(b, q);
							if (n.d < std::max(n.width / 2, L / 8) && (!best.b || n.bed < best.bed))
								best = n;
						});
					return best;
				};
				std::vector<V2> path = { p };
				std::vector<double> bed = { carve(p, k, nullptr, 0) - cut };
				Near join;
				bool stalled = false;
				for (int s2 = 0; s2 < STEPS && !join.b; s2++) {
					const V2 c = path.back();
					if ((join = joinAt(c)).b)
						break;
					const double e = L / 16;
					const V2 grad = { (carve({ c.x + e, c.y }, k, nullptr, 0) - carve({ c.x - e, c.y }, k, nullptr, 0)) / (2 * e),
						(carve({ c.x, c.y + e }, k, nullptr, 0) - carve({ c.x, c.y - e }, k, nullptr, 0)) / (2 * e) };
					const double gl = len(grad);
					if (gl < 1e-5) { stalled = true; break; }
					const V2 next = c + grad * (-L / 4 / gl);
					const double there = carve(next, k, nullptr, 0);
					if (there > carve(c, k, nullptr, 0)) { stalled = true; break; } // a hollow: no way down from here
					path.push_back(next);
					bed.push_back(std::min(bed.back(), there - cut));
				}
				if (!join.b) {
					// it never reached one: to the nearest coarser channel, as Dendry joins
					forced++;
					(void)stalled;
					double d = 1e18;
					for (int m = 0; m < k; m++)
						levels[m].around(path.back(), m == 0 ? 2 : 3, [&](const Branch &b) {
							const Near n = nearest(b, path.back());
							if (n.d < d) { d = n.d; join = n; }
						});
					if (!join.b)
						continue;
				}
				points++;
				path.push_back(join.q);
				bed.push_back(join.bed);
				for (double &v : bed)
					v = std::max(v, join.bed); // never under what it joins: it falls into it
				for (size_t q = 1; q < bed.size(); q++)
					bed[q] = std::min(bed[q], bed[q - 1]);
				Branch br;
				br.level = k;
				const double cells = (L / cell) * (L / cell) * std::max(0.05, rain.at(p));
				const double w = cells >= WET && join.width > 0 ? widthOf(cells) : 0;
				bend(br, path, bed, std::vector<double>(path.size(), w), 0, k, i, j);
				lk.add(std::move(br));
			}
		levels.push_back(std::move(lk));
	}
	const double tTree = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();

	// the land and water, a pixel at a time
	std::vector<double> h((size_t)N * N), water((size_t)N * N, -1e9);
	std::vector<int> wl((size_t)N * N, -1);
	long perched = 0, streamPx = 0, perchedAtFoot = 0;
	std::vector<double> perch;
	for (int y = 0; y < N; y++)
		for (int x = 0; x < N; x++) {
			const V2 p = { -half + (x + 0.5) * step, -half + (y + 0.5) * step };
			const size_t k = (size_t)y * N + x;
			const double ground = base.at(p);
			if (ground <= 0) { h[k] = ground; water[k] = 0; continue; }
			Near wetNear;
			const double land = carve(p, levels.size(), &wetNear, step * 0.6);
			h[k] = land;
			if (wetNear.b) {
				streamPx++;
				if (wetNear.d < wetNear.width / 2 && land < wetNear.bed - 1) {
					perched++; // the water stands above the land: it isn't in its valley
					perch.push_back(wetNear.bed - land);
					// at its foot, falling into what it joins?
					perchedAtFoot += len(wetNear.q - wetNear.b->pts.back()) < levels[wetNear.b->level].L * 0.25;
				}
				water[k] = wetNear.bed;
				wl[k] = wetNear.b->level;
				if (wetNear.d < wetNear.width / 2)
					h[k] = std::min(land, wetNear.bed - 1);
			}
		}
	const double tLand = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count() - tTree;
	std::printf("levels %d (finest %.0f blocks) · %d points, %d forced to climb (%.1f%%) · tree %.2f s, land %.1f µs a column\n",
		LEVELS, cell / std::pow(2, LEVELS), points, forced, 100.0 * forced / std::max(1, points), tTree, tLand * 1e6 / ((double)N * N));
	for (const Level &l : levels) {
		int wet = 0;
		for (const Branch &b : l.branches) wet += b.width.back() > 0;
		std::printf("  level, %5.0f blocks: %6zu branches, %5d wet\n", l.L, l.branches.size(), wet);
	}
	std::printf("stream pixels %ld, perched %.1f%% (%.0f%% of them in the last quarter-level before the foot)\n", streamPx, 100.0 * perched / std::max(1L, streamPx), 100.0 * perchedAtFoot / std::max(1L, perched));
	if (!perch.empty()) {
		std::sort(perch.begin(), perch.end());
		std::printf("  perched by, blocks: p50 %.1f p90 %.1f max %.1f\n", perch[perch.size() / 2], perch[perch.size() * 9 / 10], perch.back());
	}
	// every branch falls: check its bed along it never rises
	long rises = 0;
	for (const Level &l : levels)
		for (const Branch &b : l.branches)
			for (size_t i = 0; i + 1 < b.bed.size(); i++)
				rises += b.bed[i + 1] > b.bed[i] + 1e-9;
	std::printf("bed rises along a branch: %ld\n", rises);

	// hillshade from the north-west, height tints, contours; water blue (streams by level when LEVELCOLOURS=1)
	double lo = 1e18, hi = -1e18;
	for (double v : h) { lo = std::min(lo, v); hi = std::max(hi, v); }
	const double contour = std::max(5.0, std::pow(10, std::floor(std::log10(std::max(1.0, hi - std::max(0.0, lo)) / 4))) / 2);
	std::printf("land %.0f to %.0f blocks · contours every %.0f\n", lo, hi, contour);
	std::vector<unsigned char> rgb((size_t)N * N * 3);
	for (int y = 0; y < N; y++)
		for (int x = 0; x < N; x++) {
			const auto at = [&](int xx, int yy) { return h[(size_t)std::clamp(yy, 0, N - 1) * N + std::clamp(xx, 0, N - 1)]; };
			const size_t k = (size_t)y * N + x;
			const double gx = (at(x + 1, y) - at(x - 1, y)) / (2 * step), gz = (at(x, y + 1) - at(x, y - 1)) / (2 * step);
			const double shade = std::clamp((0.5 * gx + 0.7071 + 0.5 * gz) / std::sqrt(gx * gx + 1 + gz * gz) / 0.7071, 0.2, 1.5);
			const double e = h[k];
			double r, gg, b;
			if (water[k] > -1e8 && e <= 0) { r = 40; gg = 70; b = 120; }
			else if (wl[k] >= 0) {
				if (std::getenv("LEVELCOLOURS")) { static const double C[6][3] = { { 20, 60, 200 }, { 0, 140, 230 }, { 0, 200, 200 }, { 230, 80, 40 }, { 230, 200, 0 }, { 200, 0, 200 } }; r = C[wl[k] % 6][0]; gg = C[wl[k] % 6][1]; b = C[wl[k] % 6][2]; }
				else { r = 50; gg = 120; b = 200; }
			} else {
				const double t = std::clamp(e / std::max(50.0, hi), 0.0, 1.0);
				r = (110 + 120 * t) * shade; gg = (150 + 50 * t - 40 * t * t) * shade; b = (90 + 110 * t * t) * shade;
				const bool line = std::floor(at(x + 1, y) / contour) != std::floor(e / contour) || std::floor(at(x, y + 1) / contour) != std::floor(e / contour);
				if (line && !std::getenv("NOCONTOURS")) { r *= 0.8; gg *= 0.8; b *= 0.8; }
			}
			unsigned char *o = &rgb[k * 3];
			o[0] = (unsigned char)std::clamp(r, 0.0, 255.0); o[1] = (unsigned char)std::clamp(gg, 0.0, 255.0); o[2] = (unsigned char)std::clamp(b, 0.0, 255.0);
		}
	png(argv[2], N, N, rgb);
	return 0;
}
