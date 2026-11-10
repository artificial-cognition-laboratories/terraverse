// Standalone terragen: a region of the land at block scale scored as a
// landscape (sprint 108), so a change to how it's shaped is measured against
// real land, not only looked at:
//   scripts/terragen-tool.sh /tmp/landscape scripts/terragen-check/landscape.cpp
//   [SIZE=628320] /tmp/landscape <code> [u,v | peak | belt | plain | shield | river] [span km] [samples]
// What it reports, and what real land shows:
//   relief and slopes: local relief over an eighth of the region; slopes, and
//     the share over 45° (cliffs)
//   its own drainage (flow on the sampled ground, filled):
//     Horton's bifurcation ratio, streams of an order to the next: 3–5
//     Hack's exponent, longest stream against area drained: 0.5–0.6
//     concavity, channel slope against area drained, slope ~ A^−θ: θ 0.4–0.6
//     drainage density, channel length per area
//   lineation: how much the ground's slopes all face one way, window by
//     window (structure-tensor coherence, 0 none .. 1 ribs): the ribbing
//   hollows: closed depressions the ground holds (what ponds could fill)
//   perched: river water standing over dry ground beside it (at 1–2 blocks a sample)
#include "terragen/world.h"
#include "terragen/grid.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <queue>
#include <string>
#include <thread>
#include <vector>
using namespace terragen;
static double real(fix v) { return v / (double)ONE; }

struct Fit
{
	double sx = 0, sy = 0, sxx = 0, sxy = 0;
	long n = 0;
	void add(double x, double y) { sx += x; sy += y; sxx += x * x; sxy += x * y; n++; }
	double slope() const { return n < 3 ? NAN : (n * sxy - sx * sy) / (n * sxx - sx * sx); }
};

static double percentile(std::vector<double> v, double p)
{
	if (v.empty())
		return NAN;
	std::sort(v.begin(), v.end());
	return v[std::min(v.size() - 1, (size_t)(v.size() * p))];
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		std::fprintf(stderr, "usage: landscape <code> [u,v|peak|belt|plain|shield|river] [span km] [samples]\n");
		return 2;
	}
	WorldCode code = WorldCode::decode(argv[1]);
	if (const char *size = std::getenv("SIZE"))
		code.size = std::stoi(size);
	const std::string where = argc > 2 ? argv[2] : "belt";
	const double km = argc > 3 ? std::stod(argv[3]) : 10;
	const int N = argc > 4 ? std::stoi(argv[4]) : 512;
	std::shared_ptr<const Grid> g = gridFor(code);
	World world(code);
	const Sphere &sphere = gridSphere();
	const double R = World::radiusOf(code) / 65536.0;
	const double perM = real(blocksPerMetre(code));

	// where: as region.cpp picks
	double u0, v0;
	if (where.find(',') != std::string::npos) {
		u0 = std::stod(where.substr(0, where.find(',')));
		v0 = std::stod(where.substr(where.find(',') + 1));
	} else {
		int32_t pick = -1;
		double best = -1e18;
		for (int32_t i = 0; i < g->size(); i++) {
			if (g->base[i] < 0)
				continue;
			const double b = real(g->base[i]);
			double score = -1e18;
			if (where == "peak") score = b;
			else if (where == "belt") score = real(g->tectonics.belt[i]);
			else if (where == "dry") score = g->base[i] > ratio(5, 100) && g->rain(i) < ratio(30, 100) ? real(g->tectonics.belt[i]) : -1; // a range in dry country: fans
			else if (where == "bedded") score = (g->province[i] == terragen::Province::Basin || g->province[i] == terragen::Province::Rift) && g->base[i] > ratio(5, 100) ? real(g->tectonics.belt[i]) * (1 - real(g->tectonics.fold[i])) : -1; // flat beds rising: mesas
			else if (where == "plain") score = real(g->tectonics.distance[i]) - 4 * real(g->tectonics.belt[i]) - b * 10;
			else if (where == "shield") score = b > 0.03 && b < 0.12 && std::abs(sphere.centre(i).y) < ONE / 2 ? real(g->tectonics.stable[i]) : -1;
			else if (where == "river") score = g->river(i) && !g->lake[i] && g->base[i] > ratio(2, 100) ? (double)g->drained[i] : -1e18;
			if (score > best) { best = score; pick = i; }
		}
		const Dir d = sphere.centre(pick);
		u0 = real(World::u(d));
		v0 = real(World::v(d));
	}
	const double span = km * 1000 * perM, step = span / N;
	std::printf("%s %s at %.5f,%.5f · %.0f km (%.0f blocks) · %d² samples %.1f blocks apart\n", argv[1], where.c_str(), u0, v0, km, span, N, step);

	// the columns, in parallel by rows
	std::vector<double> H((size_t)N * N);
	std::vector<int> water((size_t)N * N);
	std::vector<uint8_t> kind((size_t)N * N);
	{
		const double lat = (0.5 - v0) * M_PI;
		const auto rows = [&](int y0, int y1) {
			for (int y = y0; y < y1; y++)
				for (int x = 0; x < N; x++) {
					const double dv = (y - N / 2) * step / (M_PI * R), du = (x - N / 2) * step / (2 * M_PI * R * std::max(0.05, std::cos(lat)));
					const double u = u0 + du - std::floor(u0 + du), v = std::clamp(v0 + dv, 0.0, 1.0);
					const double la = (0.5 - v) * M_PI, lo = (u - 0.5) * 2 * M_PI;
					const Place at((int32_t)std::llround(std::cos(la) * std::cos(lo) * PLACE_ONE), (int32_t)std::llround(std::sin(la) * PLACE_ONE),
							(int32_t)std::llround(std::cos(la) * std::sin(lo) * PLACE_ONE));
					const Ground gr = world.ground(at, world.sunlight(at, std::nullopt));
					const size_t k = (size_t)y * N + x;
					H[k] = gr.terrain;
					water[k] = gr.water;
					kind[k] = (uint8_t)gr.kind;
				}
		};
		const int T = std::max(1u, std::thread::hardware_concurrency());
		std::vector<std::thread> pool;
		for (int t = 0; t < T; t++)
			pool.emplace_back(rows, N * t / T, N * (t + 1) / T);
		for (std::thread &t : pool)
			t.join();
	}
	const auto at = [&](int x, int y) { return (size_t)y * N + x; };
	const auto sea = [&](size_t k) { return kind[k] == (uint8_t)Water::Sea; };
	long land = 0;
	for (size_t k = 0; k < H.size(); k++)
		land += !sea(k);
	if (land < N * N / 10) {
		std::printf("mostly sea: nothing to score\n");
		return 0;
	}

	// relief and slopes
	{
		std::vector<double> heights, slopes, relief;
		long cliffs = 0, sloped = 0;
		for (int y = 1; y < N - 1; y++)
			for (int x = 1; x < N - 1; x++) {
				if (sea(at(x, y)))
					continue;
				heights.push_back(H[at(x, y)]);
				const double gx = (H[at(x + 1, y)] - H[at(x - 1, y)]) / (2 * step), gy = (H[at(x, y + 1)] - H[at(x, y - 1)]) / (2 * step);
				const double s = std::atan(std::sqrt(gx * gx + gy * gy)) * 180 / M_PI;
				slopes.push_back(s);
				cliffs += s > 45;
				sloped++;
			}
		const int W = std::max(4, N / 8);
		for (int y = 0; y + W <= N; y += W / 2)
			for (int x = 0; x + W <= N; x += W / 2) {
				double lo = 1e18, hi = -1e18;
				for (int yy = y; yy < y + W; yy++)
					for (int xx = x; xx < x + W; xx++)
						if (!sea(at(xx, yy))) {
							lo = std::min(lo, H[at(xx, yy)]);
							hi = std::max(hi, H[at(xx, yy)]);
						}
				if (hi >= lo)
					relief.push_back(hi - lo);
			}
		std::printf("heights p5 %.0f p50 %.0f p95 %.0f · relief over %.1f km p50 %.0f p90 %.0f blocks · slope p50 %.1f° p90 %.1f° p99 %.1f° · cliffs (>45°) %.2f%%\n",
				percentile(heights, 0.05), percentile(heights, 0.5), percentile(heights, 0.95), km / 8, percentile(relief, 0.5), percentile(relief, 0.9),
				percentile(slopes, 0.5), percentile(slopes, 0.9), percentile(slopes, 0.99), 100.0 * cliffs / std::max(1L, sloped));
	}

	// Its own drainage: the ground filled from the sea and the region's edge
	// (priority-flood), each sample draining to the one that reached it; hollows
	// are what the fill raised
	std::vector<double> F = H;
	std::vector<int32_t> down((size_t)N * N, -1);
	std::vector<int32_t> order;
	order.reserve(H.size());
	{
		using Entry = std::pair<double, int32_t>;
		std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
		std::vector<uint8_t> seen(H.size(), 0);
		for (int y = 0; y < N; y++)
			for (int x = 0; x < N; x++)
				if (sea(at(x, y)) || x == 0 || y == 0 || x == N - 1 || y == N - 1) {
					seen[at(x, y)] = 1;
					open.push({ F[at(x, y)], (int32_t)at(x, y) });
				}
		while (!open.empty()) {
			const auto [h, k] = open.top();
			open.pop();
			order.push_back(k);
			const int x = k % N, y = k / N;
			for (int dy = -1; dy <= 1; dy++)
				for (int dx = -1; dx <= 1; dx++) {
					const int xx = x + dx, yy = y + dy;
					if ((dx == 0 && dy == 0) || xx < 0 || yy < 0 || xx >= N || yy >= N || seen[at(xx, yy)])
						continue;
					const size_t n = at(xx, yy);
					seen[n] = 1;
					F[n] = std::max(F[n], h + 1e-6);
					down[n] = k;
					open.push({ F[n], (int32_t)n });
				}
		}
	}
	// hollows: connected samples the fill raised by over half a block
	{
		std::vector<uint8_t> in(H.size(), 0);
		for (size_t k = 0; k < H.size(); k++)
			in[k] = !sea(k) && F[k] - H[k] > 0.5;
		long hollows = 0, cells = 0, pondsized = 0;
		std::vector<int32_t> stack;
		for (size_t s = 0; s < H.size(); s++) {
			if (!in[s])
				continue;
			long size = 0;
			stack.push_back((int32_t)s);
			in[s] = 0;
			while (!stack.empty()) {
				const int32_t k = stack.back();
				stack.pop_back();
				size++;
				const int x = k % N, y = k / N;
				for (int dy = -1; dy <= 1; dy++)
					for (int dx = -1; dx <= 1; dx++) {
						const int xx = x + dx, yy = y + dy;
						if (xx < 0 || yy < 0 || xx >= N || yy >= N || !in[at(xx, yy)])
							continue;
						in[at(xx, yy)] = 0;
						stack.push_back((int32_t)at(xx, yy));
					}
			}
			hollows++;
			cells += size;
			pondsized += size * step * step >= 100; // (ten blocks square or more)
		}
		long lakes = 0;
		for (size_t k = 0; k < H.size(); k++)
			lakes += kind[k] == (uint8_t)Water::Lake;
		const double area_km2 = land * step * step / (perM * perM) / 1e6;
		std::printf("hollows %ld (%ld pond-sized, %.2f per km²) over %.2f%% of the land · lake water %.2f%% of it\n", hollows, pondsized,
				pondsized / std::max(1e-9, area_km2), 100.0 * cells / land, 100.0 * lakes / land);
	}
	// area drained (samples), the longest path up (blocks), Strahler orders
	std::vector<double> area(H.size(), 1), longest(H.size(), 0);
	for (auto it = order.rbegin(); it != order.rend(); ++it) {
		const int32_t k = *it, r = down[k];
		if (r < 0)
			continue;
		area[r] += area[k];
		const double dx = (k % N) - (r % N), dy = (k / N) - (r / N);
		longest[r] = std::max(longest[r], longest[k] + std::sqrt(dx * dx + dy * dy) * step);
	}
	const double A_c = 64; // samples: what makes a channel
	std::vector<int> strahler(H.size(), 0), top_in(H.size(), 0), tops(H.size(), 0);
	std::vector<long> streams(32, 0);
	Fit hack, concavity;
	long channel = 0;
	for (int32_t k : std::vector<int32_t>(order.rbegin(), order.rend())) {
		if (area[k] < A_c || sea(k))
			continue;
		channel++;
		// (Strahler: one more than the highest in when two of it meet, else that)
		int s = top_in[k] == 0 ? 1 : top_in[k] + (tops[k] >= 2 ? 1 : 0);
		strahler[k] = s;
		const int32_t r = down[k];
		if (r >= 0 && area[r] >= A_c && !sea(r)) {
			if (s > top_in[r]) { top_in[r] = s; tops[r] = 1; }
			else if (s == top_in[r]) tops[r]++;
		}
		hack.add(std::log(area[k] * step * step), std::log(std::max(step, longest[k])));
		const int32_t rr = down[k];
		if (rr >= 0 && !sea(rr)) {
			const double dx = (k % N) - (rr % N), dy = (k / N) - (rr / N);
			const double S = (H[k] - H[rr]) / (std::sqrt(dx * dx + dy * dy) * step);
			if (S > 0)
				concavity.add(std::log(area[k] * step * step), std::log(S));
		}
	}
	// streams of each order: a channel sample whose order differs from its
	// receiver's (or whose receiver isn't a channel) ends one
	for (int32_t k : order) {
		if (strahler[k] == 0)
			continue;
		const int32_t r = down[k];
		if (r < 0 || strahler[r] != strahler[k])
			streams[strahler[k]]++;
	}
	double ratios = 0;
	int counted = 0, top = 0;
	for (int o = 1; o < 31; o++) {
		if (streams[o] > 0)
			top = o;
		if (streams[o] >= 2 && streams[o + 1] >= 2) {
			ratios += std::log((double)streams[o] / streams[o + 1]);
			counted++;
		}
	}
	std::printf("drainage (channels from %.0f samples, %.2f km²): orders to %d (", A_c, A_c * step * step / (perM * perM) / 1e6, top);
	for (int o = 1; o <= top; o++)
		std::printf("%s%ld", o > 1 ? " " : "", streams[o]);
	std::printf(") · bifurcation %.2f (3–5) · Hack %.2f (0.5–0.6) · concavity %.2f (0.4–0.6) · density %.2f km/km²\n",
			counted ? std::exp(ratios / counted) : NAN, hack.slope(), -concavity.slope(),
			channel * step / perM / 1000 / std::max(1e-9, land * step * step / (perM * perM) / 1e6));

	// lineation: structure-tensor coherence in windows of an eighth of the
	// region, weighted by how much the ground slopes there
	{
		const int W = std::max(4, N / 16);
		double sum = 0, weight = 0;
		for (int y = 1; y + W < N - 1; y += W)
			for (int x = 1; x + W < N - 1; x += W) {
				double jxx = 0, jyy = 0, jxy = 0;
				for (int yy = y; yy < y + W; yy++)
					for (int xx = x; xx < x + W; xx++) {
						if (sea(at(xx, yy)))
							continue;
						const double gx = (H[at(xx + 1, yy)] - H[at(xx - 1, yy)]) / 2, gy = (H[at(xx, yy + 1)] - H[at(xx, yy - 1)]) / 2;
						jxx += gx * gx;
						jyy += gy * gy;
						jxy += gx * gy;
					}
				const double tr = jxx + jyy;
				if (tr <= 0)
					continue;
				const double d = std::sqrt((jxx - jyy) * (jxx - jyy) + 4 * jxy * jxy);
				sum += d / tr * tr;
				weight += tr;
			}
		std::printf("lineation %.2f (0 none .. 1 ribs)\n", weight > 0 ? sum / weight : NAN);
	}

	// perched river water (only meaningful a block or two a sample)
	if (step <= 2) {
		long river = 0, perched = 0;
		for (int y = 6; y < N - 6; y++)
			for (int x = 6; x < N - 6; x++) {
				if (kind[at(x, y)] != (uint8_t)Water::River)
					continue;
				river++;
				bool over = false;
				for (int dy = -6; dy <= 6 && !over; dy++)
					for (int dx = -6; dx <= 6 && !over; dx++)
						over = kind[at(x + dx, y + dy)] == (uint8_t)Water::None && water[at(x, y)] > H[at(x + dx, y + dy)];
				perched += over;
			}
		std::printf("perched river water %.1f%% of %ld river samples\n", 100.0 * perched / std::max(1L, river), river);
	}
}
