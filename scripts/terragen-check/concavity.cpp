// Standalone terragen (no engine): how concave a world's river profiles are.
// Under stream power at steady state (uplift = erosion; fastscapelib's model,
// m/n = 0.5) channel slope falls as drained area^−0.5; this fits the exponent
// over every stream cell. Checked in sprint 89 (plates with history):
//   g++ -std=c++17 -O2 -I apps/game/src apps/game/src/terragen/*.cpp scripts/terragen-check/concavity.cpp -o concavity
//   ./concavity TF18.dGVycmE
#include "terragen/world.h"
#include "terragen/grid.h"
#include <cstdio>
#include <cmath>
using namespace terragen;
int main(int argc, char **argv) {
	World w(WorldCode::decode(argv[1])); const Grid &g = w.grid(); const Sphere &s = gridSphere();
	// all stream cells; then those being pushed up now (belt > 0.3: where
	// uplift and erosion can balance) and the rest (old land wearing down)
	struct Fit { double sx = 0, sy = 0, sxx = 0, sxy = 0; int n = 0;
		void add(double x, double y) { sx += x; sy += y; sxx += x * x; sxy += x * y; n++; }
		double slope() const { return (n * sxy - sx * sy) / (n * sxx - sx * sx); } } all, active, old;
	for (int i = 0; i < g.size(); i++) {
		int r = g.receiver[i];
		if (!g.stream(i) || r < 0 || g.lake[i] || g.eroded[r] < 0 || g.filled[i] <= g.filled[r]) continue;
		double gap = 0; const int32_t *p = s.neighboursBegin(i); const fix *d = s.distancesBegin(i);
		for (; p != s.neighboursEnd(i); p++, d++) if (*p == r) gap = *d;
		double S = (g.filled[i] - g.filled[r]) / gap, A = (double)g.drained[i];
		if (S <= 0) continue;
		all.add(std::log(A), std::log(S));
		(g.tectonics.belt[i] > ratio(3, 10) ? active : old).add(std::log(A), std::log(S));
	}
	printf("%s: %d channel cells, slope ~ area^%.2f (stream power with m/n = 0.5 predicts -0.50); rising ranges %.2f (%d), old land %.2f (%d)\n",
			argv[1], all.n, all.slope(), active.slope(), active.n, old.slope(), old.n);
}
