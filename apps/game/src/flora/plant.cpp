// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "grow.h"
#include <vector>

/*
	The plant generator: every plant from one set of genes (flora.h). A
	plant is axes bearing organs. Stems rise from its foot; each is a line
	grown a voxel at a time, bending up, over or about as its genes say; at
	each node along it, turned on from the last, it bears branches (axes one
	level down, grown the same way), leaves, or nothing; at its tip, a
	crowning cluster of leaves and a flower. A leaf is a stalk and a blade:
	an outline (how wide along its length, where widest, how it closes),
	cut by teeth, lobes, splits and holes, or made of leaflets in rows or
	in a fan; bent down along its length, cupped across it.

	A pine, an oak, a bamboo, a banana, a monstera, a fern, a tussock and a
	cactus are this one routine with different numbers.
*/
namespace flora {

namespace {

const fix GOLDEN = ratio(239996, 100000);    // radians: the turn that packs a rosette
const fix UP_PULL = R(6);                    // per voxel: how hard an upright stem straightens
const fix ARCH_PULL = R(5);                  // and an arching one bows
const fix RISE_PULL = R(5);                  // a branch turning up (or down) as it goes
const fix WIND_PULL = R(2);
const fix KINK = R(25);                      // a crooked axis's swing per voxel, at its most crooked
const fix CHILD_THINS = R(62);               // a branch's radius, of its parent's there
const fix SHORTEST = R(15);                  // a branch's length where its crown makes it least, of its longest
const fix FOOT_SPREAD = R(12);               // how far apart stems from one foot start, of its height
const fix WOOD_FROM = R(45);                 // a stem this thick (voxels) is drawn round
const fix STEP = ONE;                        // an axis grows a voxel a step; a leaf half that
const fix LEAF_STEP = HALF;
const fix TINY = R(150);                     // a leaf shorter than this (voxels) is one voxel
const fix LEAFLET = R(55);                   // a feathered leaf's leaflets: their angle off its rib, radians...
const fix LEAFLET_WIDE = R(30);              // ...their width, of their length
const fix FAN_LEAFLET_WIDE = R(14);
const fix FAN_SPAN = R(240);                 // a fan's spread, radians, at its widest
const fix HEART_TO = R(22);                  // a heart base's lobes reach this far along the blade
const fix TEETH_DEEP = R(30);                // of the half-width
const fix SPLIT_KEEP = R(25), SPLIT_GAP = R(32); // a split runs in to this of the half-width; this share of each step is cut
const fix HOLE_IN = R(35), HOLE_OUT = R(62), HOLE_GAP = R(30);
const fix CUPPED = R(35);                    // how far a cupped leaf's edges rise, of its half-width
const fix STRIPES = R(45);                   // a patterned leaf's bands, of a step
const int32_t MOST_AXES = 2500, MOST_LEAVES = 9000;

fix fraction01(fix v) { return v & (ONE - 1); }

// Two directions square to `d` and to each other.
void basis(V d, V &e1, V &e2)
{
	const V other = fabsx(d.y) < R(90) ? V{ 0, ONE, 0 } : V{ ONE, 0, 0 };
	e1 = unit(cross(other, d), { ONE, 0, 0 });
	e2 = cross(d, e1);
}

// `d` turned `angle` off itself toward the side at `around` (radians round it).
V turned(V d, fix around, fix angle)
{
	V e1, e2;
	basis(d, e1, e2);
	fix s, c, as, ac;
	sincos(around, s, c);
	sincos(angle, as, ac);
	const V side = scaled(e1, c) + scaled(e2, s);
	return unit(scaled(d, ac) + scaled(side, as));
}

class Grower
{
public:
	Grower(const Traits &t, uint32_t seed) : t(t), canvas(t.grain), rng{ seed }, seed(seed) {}

	Plant grow()
	{
		const fix height = mul(gene(Gene::Height) * t.grain, rng.in(R(88), R(112)));
		const int32_t stems = std::max(1, roundInt(gene(Gene::Stems)));
		const fix turn = rng.in(0, TWO_PI);
		for (int32_t k = 0; k < stems; k++) {
			const fix around = turn + (fix)(((int64_t)GOLDEN * k) % TWO_PI);
			fix s, c;
			sincos(around, s, c);
			// (the first in the middle; the rest round it, leaning out)
			const fix out = k ? mul(mul(height, FOOT_SPREAD), mul(gene(Gene::Splay), sqrt(div(toFix(k), toFix(stems))))) : 0;
			const fix lean = k || stems > 1 ? mul(gene(Gene::Splay), rng.in(R(60), ONE)) : mul(gene(Gene::Splay), R(15));
			const V dir = turned({ 0, ONE, 0 }, around, lean);
			const fix length = k ? mul(height, rng.in(R(70), ONE)) : height;
			axis(0, { mul(out, c), 0, mul(out, s) }, dir, length, maxx(mul(gene(Gene::Girth), height), R(30)));
		}
		// leaves over wood, flowers over all: they're laid last
		for (const Leaf &l : m_leaves)
			leaf(l.at, l.axis, l.around, l.spread, l.size, l.part);
		for (const Bud &b : m_buds)
			flower(b.at);
		return canvas.finish();
	}

private:
	struct Leaf
	{
		V at, axis;
		fix around, spread, size;
		Part part;
	};
	struct Bud
	{
		V at;
	};

	const Traits &t;
	Canvas canvas;
	Rng rng;
	uint32_t seed;
	std::vector<Leaf> m_leaves;
	std::vector<Bud> m_buds;
	int32_t m_axes = 0;

	fix gene(Gene g) const { return t.gene(g); }
	int32_t count(Gene g) const { return roundInt(gene(g)); }

	void stem(V at, fix radius)
	{
		const bool woody = gene(Gene::Woody) >= HALF;
		const Part part = woody ? Part::Wood : Part::Flesh;
		if (radius < WOOD_FROM) {
			// thinner than a voxel: a woody one's a branch as thick as it is; a green one's a voxel of stem
			canvas.set(at, woody ? Part::Branch : Part::Flesh, (uint8_t)std::clamp(roundInt(radius * 32), 3, 16));
			return;
		}
		const int32_t r = floorInt(radius) + 1;
		const int64_t limit = (int64_t)radius * radius;
		const int32_t cx = roundInt(at.x), cy = roundInt(at.y), cz = roundInt(at.z);
		for (int32_t dy = -r; dy <= r; dy++)
			for (int32_t dz = -r; dz <= r; dz++)
				for (int32_t dx = -r; dx <= r; dx++) {
					const V d{ toFix(cx + dx) - at.x, toFix(cy + dy) - at.y, toFix(cz + dz) - at.z };
					if (square(d) <= limit)
						canvas.set(cx + dx, cy + dy, cz + dz, part);
				}
		canvas.set(at, part);
	}

	// How long a branch is, by where it leaves its parent: the crown gene says where branches are longest.
	fix crowned(fix along) const
	{
		const fix peak = (gene(Gene::Crown) + ONE) / 2;
		return maxx(SHORTEST, ONE - mul(R(130), fabsx(along - peak)));
	}

	void axis(int32_t level, V at, V dir, fix length, fix radius)
	{
		if (++m_axes > MOST_AXES || length < HALF)
			return;
		const int32_t levels = count(Gene::Levels);
		const fix internode = maxx(mul(gene(Gene::Internode), length), STEP);
		const fix bare = level == 0 ? gene(Gene::Bare) : 0;
		const fix leafy_from = ONE - gene(Gene::Leafy);
		const fix kink = mul(gene(Gene::Wander), KINK);
		const fix upright = gene(Gene::Upright), rise = gene(Gene::Rise);
		const fix spiral = mul(gene(Gene::Spiral), TWO_PI);
		const int32_t per_node = std::max(1, count(Gene::PerNode)), leaves = count(Gene::Leaves);
		fix around = rng.in(0, TWO_PI), next = internode;
		const int32_t steps = std::max(1, roundInt(length));
		for (int32_t i = 1; i <= steps; i++) {
			const fix along = ratio(i, steps);
			// how it bends: up, over, with the wind, at random
			V pull{ mul(t.lean, WIND_PULL), 0, 0 };
			if (level == 0)
				pull.y = upright >= 0 ? mul(upright, UP_PULL) : -mul(mul(-upright, ARCH_PULL), along);
			else
				pull.y = mul(rise, RISE_PULL);
			pull = pull + V{ mul(kink, rng.in(-ONE, ONE)), mul(kink, rng.in(-ONE, ONE)) / 2, mul(kink, rng.in(-ONE, ONE)) };
			dir = unit(dir + pull, dir);
			at = at + scaled(dir, STEP);
			if (at.y < 0)
				at.y = 0;
			const fix here = mul(radius, ONE - mul(gene(Gene::Taper), along));
			stem(at, here);
			if (toFix(i) < next && i < steps)
				continue;
			next += internode;
			around += spiral + rng.in(-R(15), R(15));
			if (along < bare)
				continue;
			if (level < levels)
				for (int32_t k = 0; k < per_node; k++) {
					const V out = turned(dir, around + (fix)((int64_t)TWO_PI * k / per_node), gene(Gene::Angle));
					const fix reach = mul(mul(length, gene(Gene::Reach)), level == 0 ? crowned(along) : ONE - mul(along, R(60)));
					axis(level + 1, at, out, mul(reach, rng.in(R(80), R(110))), maxx(mul(here, CHILD_THINS), R(10)));
				}
			if (along >= leafy_from && t.leafing > 0)
				for (int32_t k = 0; k < leaves; k++)
					shed(at, dir, around + HALF_PI + (fix)((int64_t)TWO_PI * k / std::max(1, leaves)), gene(Gene::Spread));
		}
		// its tip: a crowning cluster, and a flower
		if (level == levels || levels == 0) {
			const int32_t crown = count(Gene::Terminal);
			for (int32_t k = 0; k < crown && t.leafing > 0; k++) {
				// the youngest stand up in the heart, the oldest lean out furthest
				const fix outer = ratio(2 * k + 1, 2 * crown);
				shed(at, dir, (fix)(((int64_t)GOLDEN * k) % TWO_PI), mul(gene(Gene::Spread), lerp(R(35), R(120), outer)));
			}
			if (t.flowering > 0 && gene(Gene::Bloom) > 0 && rng.unit() < gene(Gene::Bloom))
				m_buds.push_back({ at });
		}
	}

	void shed(V at, V axis, fix around, fix spread)
	{
		if ((int32_t)m_leaves.size() >= MOST_LEAVES)
			return;
		const fix size = mul(gene(Gene::Size) * t.grain, rng.in(R(85), R(115)));
		m_leaves.push_back({ at, axis, around, spread, mul(size, lerp(R(40), ONE, t.leafing)), Part::Leaf });
	}

	// The blade's half-width at `b` (0 its base .. 1 its tip), of the whole leaf's widest.
	fix outline(fix b) const
	{
		const fix widest = gene(Gene::Widest);
		fix w;
		if (b < widest)
			w = sqrt(sin(mul(HALF_PI, div(b, widest))));
		else {
			const fix x = div(b - widest, ONE - widest);
			w = x <= 0 ? ONE : ONE - pow(maxx(x, 1), gene(Gene::Point));
		}
		if (gene(Gene::Heart) > 0 && b < HEART_TO)
			w = maxx(w, mul(gene(Gene::Heart), ONE - div(b, HEART_TO)));
		if (gene(Gene::Lobes) > 0)
			w = mul(w, ONE - mul(gene(Gene::Cut), HALF - cos(mul(mul(TWO_PI, b), gene(Gene::Lobes))) / 2));
		return maxx(w, 0);
	}

	// A blade from `at` along `dir`, its sides toward `side`: `length` voxels, `wide` its widest half-width.
	// `whole`: its edge is cut as the genes say (leaflets aren't).
	void blade(V at, V dir, V side, fix length, fix wide, bool whole)
	{
		const fix droop = gene(Gene::Droop), curl = gene(Gene::Curl), variegate = gene(Gene::Variegate);
		const int32_t thick = std::max(1, count(Gene::Thick));
		const fix steps = maxx(div(length, R(300)), toFix(3)); // splits and holes: one each this many voxels
		V pos = at;
		const V down{ 0, -ONE, 0 };
		for (fix s = 0; s <= length; s += LEAF_STEP) {
			const fix b = div(s, length);
			const V way = unit(dir + scaled(down, mul(droop, mul(b, sqrt(b)))), dir);
			pos = pos + scaled(way, LEAF_STEP);
			const fix w = mul(wide, outline(b));
			const fix cut = fraction01(mul(b, steps));
			for (fix v = -w; v <= w; v += LEAF_STEP) {
				const fix across = w > 0 ? div(fabsx(v), w) : 0;
				if (whole) {
					if (gene(Gene::Teeth) > 0 && across > ONE - mul(mul(gene(Gene::Teeth), TEETH_DEEP), fraction01(mul(s, R(50)))))
						continue;
					if (gene(Gene::Splits) > 0 && across > ONE - mul(gene(Gene::Splits), ONE - SPLIT_KEEP) && cut < SPLIT_GAP)
						continue;
					if (gene(Gene::Holes) > 0 && across > HOLE_IN && across < HOLE_OUT && fraction01(cut + HALF) < mul(gene(Gene::Holes), HOLE_GAP))
						continue;
				}
				// its rib, and its pattern: bands across, in its second colour
				Part part = Part::Leaf;
				if (fabsx(v) < R(60) && wide > R(250) && variegate > R(4))
					part = Part::Vein;
				else if (variegate > R(20) && wide > R(150) && fraction01(mul(s, R(30))) < mul(variegate, STRIPES) && across < R(80))
					part = Part::Vein;
				const V point = pos + scaled(side, v) + V{ 0, mul(mul(curl, CUPPED), mul(across, fabsx(v))), 0 };
				for (int32_t k = 0; k < thick; k++)
					canvas.set(point + V{ 0, -toFix(k), 0 }, part);
			}
		}
	}

	void leaf(V at, V axis, fix around, fix spread, fix size, Part)
	{
		const V out = turned(axis, around, spread);
		if (size < TINY) {
			// (a needle or a scale: a short run of leaf off its twig, so a crown of them reads as one)
			canvas.set(at + out, Part::Leaf);
			canvas.set(at + scaled(out, toFix(2)), Part::Leaf);
			return;
		}
		const fix stalk = mul(size, gene(Gene::Stalk));
		const V down{ 0, -ONE, 0 };
		V pos = at, way = out;
		// the stalk, bending with the leaf
		for (fix s = 0; s < stalk; s += LEAF_STEP) {
			const fix b = div(s, stalk + size);
			way = unit(out + scaled(down, mul(gene(Gene::Droop), mul(b, sqrt(b)))), out);
			pos = pos + scaled(way, LEAF_STEP);
			canvas.set(pos, Part::Flesh);
		}
		V side = unit(cross({ 0, ONE, 0 }, way), V{ ONE, 0, 0 });
		const int32_t kind = count(Gene::Compound);
		if (kind == 0) {
			blade(pos, way, side, size, mul(size, gene(Gene::Width)) / 2, true);
			return;
		}
		if (kind == 2) {
			// a fan: leaflets spread from the stalk's end, in its plane
			const int32_t n = std::max(3, count(Gene::Leaflets));
			const fix span = mul(FAN_SPAN, gene(Gene::Width));
			for (int32_t k = 0; k < n; k++) {
				const fix a = -span / 2 + (fix)((int64_t)span * k / (n - 1));
				fix as, ac;
				sincos(a, as, ac);
				const V d = unit(scaled(way, ac) + scaled(side, as), way);
				blade(pos, d, unit(cross({ 0, ONE, 0 }, d), side), size, mul(size, FAN_LEAFLET_WIDE) / 2, false);
			}
			return;
		}
		// feathered: a rib, and leaflets in pairs along it, longest where the outline's widest
		const int32_t n = std::max(2, count(Gene::Leaflets) / 2);
		const fix droop = gene(Gene::Droop);
		V rib = pos;
		int32_t placed = 0;
		for (fix s = 0; s <= size; s += LEAF_STEP) {
			const fix b = div(s, size);
			const V d = unit(way + scaled(down, mul(droop, mul(b, sqrt(b)))), way);
			rib = rib + scaled(d, LEAF_STEP);
			canvas.set(rib, Part::Flesh);
			if (b * n < toFix(placed) || placed >= n)
				continue;
			placed++;
			const fix leaflet = mul(mul(mul(size, gene(Gene::Width)), R(160)), maxx(outline(b), R(15)));
			const V across = unit(cross({ 0, ONE, 0 }, d), side);
			for (const int sign : { -1, 1 }) {
				fix ls, lc;
				sincos(LEAFLET, ls, lc);
				const V ld = unit(scaled(d, lc) + scaled(across, sign * ls), d);
				blade(rib, ld, unit(cross({ 0, ONE, 0 }, ld), across), leaflet, mul(leaflet, LEAFLET_WIDE) / 2, false);
			}
		}
	}

	void flower(V at)
	{
		const fix raised = gene(Gene::Raised) * t.grain;
		V pos = at;
		for (fix s = 0; s < raised; s += STEP) {
			pos = pos + V{ 0, ONE, 0 };
			canvas.set(pos, Part::Flesh);
		}
		const fix r = gene(Gene::Head) * t.grain / 2;
		if (r < R(60)) {
			canvas.set(pos + V{ 0, ONE, 0 }, Part::Flower);
			return;
		}
		const int32_t n = floorInt(r) + 1;
		const int64_t limit = (int64_t)r * r;
		for (int32_t dy = 0; dy <= n; dy++)
			for (int32_t dz = -n; dz <= n; dz++)
				for (int32_t dx = -n; dx <= n; dx++)
					if ((int64_t)toFix(dx) * toFix(dx) + (int64_t)toFix(dz) * toFix(dz) + (int64_t)toFix(dy) * toFix(dy) * 4 <= limit)
						canvas.set(roundInt(pos.x) + dx, roundInt(pos.y) + 1 + dy, roundInt(pos.z) + dz, Part::Flower);
	}
};

} // namespace

Plant growPlant(const Traits &t, uint32_t seed)
{
	return Grower(t, seed).grow();
}

} // namespace flora
