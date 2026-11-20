// Standalone terragen (no engine) against fastscapelib: the grid's erosion
// beside a reference landscape-evolution model run from the same land, on
// the same cells, under the same uplift and erodibility. A local check only:
// fastscapelib is GPL-3, so it's fetched into .output/oracle and never
// shipped, and nothing of it is copied into terragen (AGENTS.md, Licensing).
//   bash scripts/terragen-oracle.sh TF24.dGVycmE out.png [rounds]
// (out.png "-": no picture.) Exits 1 if the gate fails (GATE below); `bun
// check:oracle` runs it over a few codes.
// Two comparisons. The gate (sprint 116, phase 1): terragen's erosion core
// (drainage.h, erosion.h) in fixed point and fastscapelib, in lockstep from
// the same land, must drain the same way and stand at the same heights.
// Then, for context, terragen's finished land (sediment, creep, lakes and
// all) beside both. Each: hypsometry, concavity, heights, receivers (one
// draining into the sea drains into the sea, by whichever of its cells),
// which cells carry rivers. The core runs here with fastscapelib's
// convention for lakes (Lakes::Bed), so both solve the same equations; the
// game's is Lakes::Base (erosion.h). The PNG stacks terragen's finished
// land, the core's and fastscapelib's after the last round.
#include "library/world/terragen/erosion.h"
#include "library/world/terragen/grid.h"
#include "library/world/terragen/world.h"
#include "src/shared/util/png.h"

#include <xtensor/containers/xtensor.hpp>
#include "fastscapelib/eroders/spl.hpp"
#include "fastscapelib/flow/flow_graph.hpp"
#include "fastscapelib/flow/flow_router.hpp"
#include "fastscapelib/flow/sink_resolver.hpp"
#include "fastscapelib/grid/base.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace terragen;
namespace fs = fastscapelib;

// The grid's sphere as a fastscapelib grid: its own cells, neighbours, the
// distances between them (in mean spacings) and areas (in mean cells), so
// both models see the same mesh. The sea's cells are its base levels.
class sphere_grid;

template <>
struct fs::grid_inner_types<sphere_grid>
{
	static constexpr bool is_structured = false;
	static constexpr bool is_uniform = false;
	using grid_data_type = double;
	using container_selector = fs::xt_selector;
	static constexpr std::size_t container_ndims = 1;
	static constexpr uint8_t n_neighbors_max = 6;
	using neighbors_cache_type = fs::neighbors_no_cache<0>;
};

class sphere_grid : public fs::grid<sphere_grid>
{
public:
	using base_type = fs::grid<sphere_grid>;
	using size_type = typename base_type::size_type;
	using shape_type = typename base_type::shape_type;
	using grid_data_type = typename base_type::grid_data_type;
	using nodes_status_type = typename base_type::nodes_status_type;
	using container_type = fs::fixed_shape_container_t<fs::xt_selector, double, 1>;

	sphere_grid(const Sphere &sphere, const std::vector<uint8_t> &sea) :
		base_type((std::size_t)sphere.size()), m_size((size_type)sphere.size()), m_shape({ m_size })
	{
		m_nodes_status = nodes_status_type(m_shape, fs::node_status::core);
		m_areas = container_type(m_shape);
		m_indices.resize(m_size);
		m_distances.resize(m_size);
		const double spacing = sphere.spacing() / 65536.0;
		for (int32_t i = 0; i < sphere.size(); i++) {
			if (sea[i])
				m_nodes_status(i) = fs::node_status::fixed_value;
			m_areas(i) = sphere.area(i) / 65536.0;
			const fix *d = sphere.distancesBegin(i);
			for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++, d++) {
				m_indices[i].push_back((size_type)*p);
				m_distances[i].push_back(*d / 65536.0 / spacing);
			}
		}
	}

private:
	size_type m_size;
	shape_type m_shape;
	nodes_status_type m_nodes_status;
	container_type m_areas;
	std::vector<neighbors_indices_impl_type> m_indices;
	std::vector<neighbors_distances_impl_type> m_distances;

	container_type nodes_areas_impl() const { return m_areas; }
	grid_data_type nodes_areas_impl(const size_type &idx) const noexcept { return m_areas(idx); }
	size_type neighbors_count_impl(const size_type &idx) const { return m_indices[idx].size(); }
	void neighbors_indices_impl(neighbors_indices_impl_type &neighbors, const size_type &idx) const { neighbors = m_indices[idx]; }
	const neighbors_distances_impl_type &neighbors_distances_impl(const size_type &idx) const { return m_distances[idx]; }
	static constexpr std::size_t dimension_impl() noexcept { return 2; }

	friend class fs::grid<sphere_grid>;
};

// Mirrors grid.cpp's erosion constants (per round): stream power with m = ½,
// n = 1, K the ERODIBILITY a plateau holds to a twentieth of, uplift UPLIFT ×
// belt off the plateaus. Drained area is rain-weighted, as the grid's is.
static const double ERODIBILITY = 6.0 / 100, UPLIFT = 1.0 / 200;
static const double METRES = 5000; // a unit of height

struct Land
{
	std::string name;
	std::vector<double> h, area; // area: rain-weighted, in mean cells
	std::vector<int64_t> down;   // −1: the sea
	double concavity = 0;        // slope ~ area^this along the streams
};

// The gate. Rounding differs (fixed point against floats), so near-ties on
// flat lowland flip receivers and the runs wander apart there over hundreds
// of rounds: drainage is held to the reference early, heights and profiles
// over the whole run.
struct Agreement
{
	double rms_m, correlation, same, rivers; // the last two as shares
};
static const double EARLY_SAME = 0.97, EARLY_RIVERS = 0.93, EARLY_RMS_M = 20;
static const double LATE_CORRELATION = 0.998, LATE_RMS_M = 30, CONCAVITY_APART = 0.05;

int main(int argc, char **argv)
{
	if (argc < 3) {
		std::fprintf(stderr, "usage: oracle <code> <out.png|-> [rounds]\n");
		return 2;
	}
	const WorldCode code = WorldCode::decode(argv[1]);
	const int rounds_most = argc > 3 ? std::stoi(argv[3]) : 400;
	const std::shared_ptr<const Grid> kept = gridFor(code);
	const Grid &g = *kept;
	const Sphere &sphere = gridSphere();
	const int32_t n = sphere.size();

	// the inputs, made as grid.cpp makes them, in fixed point for terragen's
	// core and in floats for fastscapelib (the sea's surface its base level)
	std::vector<uint8_t> sea(n);
	std::vector<fix> fh(n), fup(n), fk(n), frain(n);
	auto base = xt::xtensor<double, 1>::from_shape({ (size_t)n });
	auto uplift = base, k = base, rain = base;
	for (int32_t i = 0; i < n; i++) {
		sea[i] = g.base[i] < 0;
		const fix plateau = g.tectonics.plateau[i];
		fh[i] = g.base[i];
		fup[i] = sea[i] ? 0 : mul(mul(ratio(1, 200), g.tectonics.belt[i]), ONE - plateau);
		fk[i] = mul(ratio(6, 100), ONE - plateau * 19 / 20);
		frain[i] = clamp(g.rain(i), ratio(15, 100), toFix(3));
		base(i) = sea[i] ? 0 : fh[i] / 65536.0;
		uplift(i) = fup[i] / 65536.0;
		k(i) = fk[i] / 65536.0;
		rain(i) = frain[i] / 65536.0; // (per area: fastscapelib's accumulate weighs it by the cell's)
	}

	sphere_grid grid(sphere, sea);
	fs::flow_graph<sphere_grid> flow(grid, { fs::single_flow_router(), fs::mst_sink_resolver() });
	auto spl = fs::make_spl_eroder(flow, k, 0.5, 1.0, 1e-5);

	auto snapshot = [&](const std::string &name, const xt::xtensor<double, 1> &h) {
		flow.update_routes(h);
		const auto acc = flow.accumulate(rain);
		const auto &recv = flow.impl().receivers();
		Land l{ name, std::vector<double>(n), std::vector<double>(n), std::vector<int64_t>(n) };
		for (int32_t i = 0; i < n; i++) {
			l.h[i] = sea[i] ? g.base[i] / 65536.0 : h(i);
			l.area[i] = acc(i);
			l.down[i] = sea[i] || recv(i, 0) == (size_t)i || sea[recv(i, 0)] ? -1 : (int64_t)recv(i, 0);
		}
		return l;
	};
	auto core = [&](const std::string &name) {
		const Routing way = route(fh);
		const std::vector<int64_t> acc = accumulate(way.receiver, way.order, frain);
		Land l{ name, std::vector<double>(n), std::vector<double>(n), std::vector<int64_t>(n) };
		for (int32_t i = 0; i < n; i++) {
			l.h[i] = fh[i] / 65536.0;
			l.area[i] = acc[i] / 65536.0;
			l.down[i] = sea[i] || sea[way.receiver[i]] ? -1 : way.receiver[i];
		}
		return l;
	};

	// a round of uplift and implicit stream power in each
	xt::xtensor<double, 1> h = base;
	auto round = [&]() {
		const xt::xtensor<double, 1> up = h + uplift;
		flow.update_routes(up);
		const auto acc = flow.accumulate(rain);
		const auto &eroded = spl.erode(up, acc, 1.0);
		for (int32_t i = 0; i < n; i++)
			if (!sea[i])
				h(i) = std::max(up(i) - eroded(i), 0.0);
		for (int32_t i = 0; i < n; i++)
			fh[i] += fup[i];
		const Routing way = route(fh);
		streamPower(fh, way, accumulate(way.receiver, way.order, frain), fk, nullptr, Lakes::Bed);
	};

	std::vector<Land> lands;
	{
		Land t{ "terragen", std::vector<double>(n), std::vector<double>(n), std::vector<int64_t>(n) };
		for (int32_t i = 0; i < n; i++) {
			t.h[i] = g.eroded[i] / 65536.0;
			t.area[i] = g.drained[i] / 65536.0;
			t.down[i] = sea[i] || g.receiver[i] < 0 || sea[g.receiver[i]] ? -1 : g.receiver[i];
		}
		lands.push_back(t);
	}
	// (before any round, and after one: routing alone, then one step of erosion)
	lands.push_back(core("core 0"));
	lands.push_back(snapshot("fastscape 0", h));
	round();
	lands.push_back(core("core 1"));
	lands.push_back(snapshot("fastscape 1", h));
	for (int r = 1; r < 12; r++)
		round();
	lands.push_back(core("core 12"));
	lands.push_back(snapshot("fastscape 12", h));
	for (int r = 12; r < rounds_most; r++)
		round();
	const std::string last = std::to_string(rounds_most);
	lands.push_back(core("core " + last));
	lands.push_back(snapshot("fastscape " + last, h));
	std::printf("%s: %d cells, %d rounds\n", argv[1], n, rounds_most);

	// a river: a cell draining at least the grid's river threshold of rain
	const double river = g.river_threshold / 65536.0;
	for (Land &l : lands) {
		double land = 0, low = 0, high = 0, top = 0, sx = 0, sy = 0, sxx = 0, sxy = 0;
		int fit = 0, rivers = 0;
		std::vector<double> heights;
		for (int32_t i = 0; i < n; i++) {
			if (sea[i])
				continue;
			const double a = sphere.area(i) / 65536.0;
			land += a;
			low += l.h[i] < 0.1 ? a : 0;
			high += l.h[i] > 0.4 ? a : 0;
			top = std::max(top, l.h[i]);
			heights.push_back(l.h[i]);
			rivers += l.area[i] >= river;
			const int64_t r = l.down[i];
			if (r < 0 || sea[r] || l.area[i] < river / 4)
				continue;
			double gap = 0;
			const fix *d = sphere.distancesBegin(i);
			for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++, d++)
				if (*p == r)
					gap = *d / (double)sphere.spacing();
			const double S = (l.h[i] - l.h[r]) / gap;
			if (S <= 0)
				continue;
			const double x = std::log(l.area[i]), y = std::log(S);
			sx += x; sy += y; sxx += x * x; sxy += x * y; fit++;
		}
		l.concavity = (fit * sxy - sx * sy) / (fit * sxx - sx * sx);
		std::sort(heights.begin(), heights.end());
		auto at = [&](double q) { return heights[(size_t)(q * (heights.size() - 1))] * METRES; };
		std::printf("  %-16s land under 500 m %3.0f%%, over 2 km %3.0f%% · median %5.0f m, 90%% %5.0f m, top %5.0f m · slope ~ area^%.2f · river cells %d\n",
				l.name.c_str(), 100 * low / land, 100 * high / land, at(0.5), at(0.9), top * METRES,
				l.concavity, rivers);
	}
	// a pair's agreement over the land: heights, receivers, river cells
	auto compare = [&](const Land &t, const Land &l) {
		double sq = 0, st = 0, sl = 0, stt = 0, sll = 0, stl = 0;
		int cells = 0, same = 0, both = 0, either = 0;
		for (int32_t i = 0; i < n; i++) {
			if (sea[i])
				continue;
			const double a = t.h[i], b = l.h[i];
			sq += (a - b) * (a - b); st += a; sl += b; stt += a * a; sll += b * b; stl += a * b;
			cells++;
			same += t.down[i] == l.down[i];
			const bool rt = t.area[i] >= river, rl = l.area[i] >= river;
			both += rt && rl;
			either += rt || rl;
		}
		const Agreement a{ std::sqrt(sq / cells) * METRES,
			(cells * stl - st * sl) / std::sqrt((cells * stt - st * st) * (cells * sll - sl * sl)),
			(double)same / cells, (double)both / std::max(1, either) };
		std::printf("  %-14s vs %-14s height rms %6.1f m, correlation %.4f · same receiver %5.1f%% · river cells shared %5.1f%%\n",
				t.name.c_str(), l.name.c_str(), a.rms_m, a.correlation, 100 * a.same, 100 * a.rivers);
		return a;
	};
	std::printf("gate (core against fastscapelib):\n");
	std::vector<Agreement> agree;
	for (size_t k = 1; k + 1 < lands.size(); k += 2)
		agree.push_back(compare(lands[k], lands[k + 1]));
	std::printf("context (terragen's finished land):\n");
	compare(lands[0], lands[6]);
	const Agreement &early = agree[2], &late = agree[3];
	const double apart = std::abs(lands[7].concavity - lands[8].concavity);
	std::vector<std::string> failed;
	if (early.same < EARLY_SAME || early.rivers < EARLY_RIVERS || early.rms_m > EARLY_RMS_M)
		failed.push_back("after 12 rounds, drainage or heights apart");
	if (late.correlation < LATE_CORRELATION || late.rms_m > LATE_RMS_M)
		failed.push_back("after " + last + " rounds, heights apart");
	if (apart > CONCAVITY_APART)
		failed.push_back("after " + last + " rounds, river profiles apart");
	for (const std::string &f : failed)
		std::printf("FAIL %s: %s\n", argv[1], f.c_str());
	if (failed.empty())
		std::printf("ok %s\n", argv[1]);

	if (std::string(argv[2]) == "-")
		return failed.empty() ? 0 : 1;
	// three stacked, equirectangular: shaded relief, rivers in blue
	const std::vector<const Land *> shown{ &lands[0], &lands[7], &lands[8] };
	const int W = 768, H = 384;
	std::vector<unsigned char> rgb((size_t)W * H * 3 * shown.size());
	for (size_t k = 0; k < shown.size(); k++) {
		const Land &l = *shown[k];
		for (int y = 0; y < H; y++)
			for (int x = 0; x < W; x++) {
				auto cell = [&](int px, int py) {
					px = (px + W) % W;
					py = std::clamp(py, 0, H - 1);
					return sphere.cell(World::place(ratio(2 * px + 1, 2 * W), ratio(2 * py + 1, 2 * H)).dir());
				};
				const int32_t i = cell(x, y), ie = cell(x - 1, y - 1);
				const double e = l.h[i], shade = 1 + (l.h[ie] - e) * 18;
				double r, gg, b;
				if (sea[i]) { const double d = std::max(0.0, 1 + e * 1.4); r = 20 * d; gg = 50 + 50 * d; b = 110 + 70 * d; }
				else if (l.area[i] >= river) { r = 40; gg = 90; b = 200; }
				else if (e < 0.15) { r = (90 + 200 * e) * shade; gg = (150 + 100 * e) * shade; b = 70 * shade; }
				else if (e < 0.45) { r = (120 + 100 * (e - 0.15)) * shade; gg = (160 - 120 * (e - 0.15)) * shade; b = 80 * shade; }
				else { const double s = std::min(1.0, (e - 0.45) * 2); r = (140 + 100 * s) * shade; gg = (125 + 115 * s) * shade; b = (115 + 125 * s) * shade; }
				unsigned char *px = &rgb[(((size_t)k * H + y) * W + x) * 3];
				px[0] = (unsigned char)std::clamp(r, 0.0, 255.0);
				px[1] = (unsigned char)std::clamp(gg, 0.0, 255.0);
				px[2] = (unsigned char)std::clamp(b, 0.0, 255.0);
			}
	}
	png(argv[2], W, H * (int)shown.size(), rgb);
	std::printf("wrote %s (top to bottom: %s, %s, %s)\n", argv[2], shown[0]->name.c_str(), shown[1]->name.c_str(), shown[2]->name.c_str());
	return failed.empty() ? 0 : 1;
}
