// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "aero.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <set>
#include <utility>

namespace physics {
namespace {

constexpr float PI_F = 3.14159265f;
constexpr int STRIPS_A_SURFACE = 8; // slices side by side that push as one

struct Cells
{
	int lo[3], hi[3]; // half open
};

struct Column
{
	int f, lo, hi; // along the chord; across it, half open
};

// What's joined in one slice: a run along the chord, one piece across it at each step.
struct Section
{
	std::vector<Column> columns;
	int wing = -1;
	int flo() const { return columns.front().f; }
	int fhi() const { return columns.back().f + 1; }
	int nlo() const
	{
		int v = columns[0].lo;
		for (const Column &c : columns)
			v = std::min(v, c.lo);
		return v;
	}
	int nhi() const
	{
		int v = columns[0].hi;
		for (const Column &c : columns)
			v = std::max(v, c.hi);
		return v;
	}
};

float smoothstep(float a, float b, float x)
{
	const float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
	return t * t * (3 - 2 * t);
}

// The pieces across the chord at each step along it, in one slice.
std::vector<Section> sectionsOf(std::map<int, std::vector<std::pair<int, int>>> &columns)
{
	std::vector<Section> done, open;
	int last = 0;
	for (auto &[f, runs] : columns) {
		std::sort(runs.begin(), runs.end());
		std::vector<std::pair<int, int>> merged;
		for (const auto &r : runs) {
			if (!merged.empty() && r.first <= merged.back().second)
				merged.back().second = std::max(merged.back().second, r.second);
			else
				merged.push_back(r);
		}
		if (f != last + 1) {
			done.insert(done.end(), open.begin(), open.end());
			open.clear();
		}
		std::vector<Section> next;
		for (const auto &m : merged) {
			// on from the section it touches; a stair's next step touches
			auto it = std::find_if(open.begin(), open.end(), [&](const Section &s) {
				const Column &c = s.columns.back();
				return m.first <= c.hi && c.lo <= m.second;
			});
			Section s;
			if (it != open.end()) {
				s = std::move(*it);
				open.erase(it);
			}
			s.columns.push_back({ f, m.first, m.second });
			next.push_back(std::move(s));
		}
		done.insert(done.end(), open.begin(), open.end());
		open = std::move(next);
		last = f;
	}
	done.insert(done.end(), open.begin(), open.end());
	return done;
}

struct Slice
{
	float area, chord_at, across_at, along_at, incidence, camber, thickness, chord;
};

Slice measure(const Section &s, float cell)
{
	// the middle line, fitted straight: its slope is the incidence, how far
	// its middle stands off that is its camber
	float n = 0, fx = 0, nx = 0, ff = 0, fn = 0;
	int thick = 0;
	for (const Column &c : s.columns) {
		const float f = c.f + 0.5f, m = (c.lo + c.hi) * 0.5f;
		n++;
		fx += f;
		nx += m;
		ff += f * f;
		fn += f * m;
		thick = std::max(thick, c.hi - c.lo);
	}
	const float denom = n * ff - fx * fx;
	const float slope = denom > 0 ? (n * fn - fx * nx) / denom : 0;
	const float mf = fx / n, mn = nx / n;
	float bow = 0;
	for (const Column &c : s.columns)
		if (std::abs(c.f + 0.5f - mf) < std::max(1.f, n / 4))
			bow = std::max(bow, (c.lo + c.hi) * 0.5f - (mn + slope * (c.f + 0.5f - mf)));
	const float chord = n * cell;
	Slice out;
	out.chord = chord;
	out.area = chord * cell;
	out.incidence = std::atan(slope);
	out.camber = bow * cell / chord;
	out.thickness = thick * cell / chord;
	const float quarter = s.fhi() - n / 4; // from its front
	out.chord_at = quarter * cell;
	out.across_at = (mn + slope * (quarter - mf)) * cell;
	out.along_at = 0;
	return out;
}

} // namespace

Aero aeroOf(const std::vector<aabb3f> &boxes, v3f ahead)
{
	Aero out;
	if (boxes.empty())
		return out;
	aabb3f whole = boxes[0];
	for (const aabb3f &b : boxes)
		whole.addInternalBox(b);
	const v3f size = whole.getExtent();
	// a sixteenth, coarser for what's enormous (rubble of a hillside)
	float cell = 1 / 16.f;
	while (std::max({ size.X * size.Y, size.Y * size.Z, size.Z * size.X }) / (cell * cell) > 4e6f)
		cell *= 2;
	std::vector<Cells> cells;
	cells.reserve(boxes.size());
	for (const aabb3f &b : boxes) {
		Cells c;
		for (int a = 0; a < 3; a++) {
			c.lo[a] = (int)std::lround(b.MinEdge[a] / cell);
			c.hi[a] = (int)std::lround(b.MaxEdge[a] / cell);
			if (c.hi[a] <= c.lo[a]) // finer than a cell: still something
				c.hi[a] = c.lo[a] + 1;
		}
		cells.push_back(c);
	}

	// each silhouette, a line of runs at a time
	for (int axis = 0; axis < 3; axis++) {
		const int u = (axis + 1) % 3, w = (axis + 2) % 3;
		std::map<int, std::vector<std::pair<int, int>>> rows;
		for (const Cells &c : cells)
			for (int i = c.lo[u]; i < c.hi[u]; i++)
				rows[i].push_back({ c.lo[w], c.hi[w] });
		double area = 0, mu = 0, mw = 0;
		for (auto &[i, runs] : rows) {
			std::sort(runs.begin(), runs.end());
			int at = runs[0].first, end = runs[0].first;
			auto close = [&]() {
				const double n = end - at;
				area += n;
				mu += n * (i + 0.5);
				mw += n * (at + end) * 0.5;
			};
			for (const auto &r : runs) {
				if (r.first > end) {
					close();
					at = r.first;
					end = r.first;
				}
				end = std::max(end, r.second);
			}
			close();
		}
		out.shown[axis] = (float)area * cell * cell;
		v3f centre = whole.getCenter();
		if (area > 0) {
			centre[u] = (float)(mu / area) * cell;
			centre[w] = (float)(mw / area) * cell;
		}
		out.centre[axis] = centre;
	}

	int f = 2;
	if (std::abs(ahead.X) >= std::abs(ahead.Y) && std::abs(ahead.X) >= std::abs(ahead.Z))
		f = 0;
	else if (std::abs(ahead.Y) >= std::abs(ahead.Z))
		f = 1;
	const float fsign = ahead[f] < 0 ? -1 : 1;

	// what of the front's silhouette is thin sections, which part the air rather than stop it
	float parted[2] = { 0, 0 };
	// spanning each way across the front: the other way is its normal
	for (int view = 0; view < 2; view++) {
		const int s = (f + 1 + view) % 3, n = (f + 2 - view) % 3;
		// along the chord counted from the back, so a section reads the same either way round
		std::map<int, std::map<int, std::vector<std::pair<int, int>>>> strips;
		for (const Cells &c : cells) {
			const int flo = fsign > 0 ? c.lo[f] : -c.hi[f], fhi = fsign > 0 ? c.hi[f] : -c.lo[f];
			for (int i = c.lo[s]; i < c.hi[s]; i++)
				for (int j = flo; j < fhi; j++)
					strips[i][j].push_back({ c.lo[n], c.hi[n] });
		}
		// slices side by side, joined into wings
		std::map<int, std::vector<Section>> slices;
		std::vector<int> parent;
		auto find = [&](int a) {
			while (parent[a] != a)
				a = parent[a] = parent[parent[a]];
			return a;
		};
		for (auto &[i, columns] : strips) {
			std::vector<Section> here = sectionsOf(columns);
			const auto before = slices.find(i - 1);
			for (Section &sec : here) {
				if (before != slices.end())
					for (const Section &o : before->second)
						if (sec.flo() < o.fhi() && o.flo() < sec.fhi() && sec.nlo() <= o.nhi() && o.nlo() <= sec.nhi()) {
							if (sec.wing < 0)
								sec.wing = o.wing;
							else
								parent[find(o.wing)] = find(sec.wing);
						}
				if (sec.wing < 0) {
					sec.wing = (int)parent.size();
					parent.push_back(sec.wing);
				}
			}
			slices[i] = std::move(here);
		}
		// a wing's area and span
		std::map<int, float> wing_area;
		std::map<int, std::set<int>> wing_span;
		for (auto &[i, here] : slices)
			for (const Section &sec : here) {
				wing_area[find(sec.wing)] += measure(sec, cell).area;
				wing_span[find(sec.wing)].insert(i);
			}
		// a surface for every few slices of a wing
		struct Open
		{
			Surface sum{};
			float at_f = 0, at_n = 0, at_s = 0;
			int strips = 0;
		};
		std::map<int, Open> open;
		auto close = [&](int wing) {
			Open &o = open[wing];
			if (o.strips == 0)
				return;
			const float a = o.sum.area;
			Surface r = o.sum;
			r.incidence /= a;
			r.camber /= a;
			r.thickness /= a;
			r.at[f] = fsign * o.at_f / a;
			r.at[n] = o.at_n / a;
			r.at[s] = o.at_s / a;
			r.normal = v3f(0);
			r.normal[n] = 1;
			r.chord = v3f(0);
			r.chord[f] = fsign;
			const float span = wing_span[wing].size() * cell;
			r.aspect = span * span / wing_area[wing];
			// past this it's a block in the wind: no lift to speak of
			if (r.thickness < 0.6f)
				out.surfaces.push_back(r);
			o = Open{};
		};
		for (auto &[i, here] : slices) {
			for (const Section &sec : here) {
				const int wing = find(sec.wing);
				const Slice m = measure(sec, cell);
				Open &o = open[wing];
				o.sum.area += m.area;
				o.sum.incidence += m.incidence * m.area;
				o.sum.camber += m.camber * m.area;
				o.sum.thickness += m.thickness * m.area;
				o.at_f += m.chord_at * m.area;
				o.at_n += m.across_at * m.area;
				o.at_s += (i + 0.5f) * cell * m.area;
				if (m.thickness < 0.3f)
					parted[view] += (sec.nhi() - sec.nlo()) * cell * cell;
				if (++o.strips >= STRIPS_A_SURFACE)
					close(wing);
			}
		}
		for (auto &[wing, o] : open)
			close(wing);
	}
	// each wing shown twice is still shown once; the blunt ends of it remain
	out.shown[f] = std::max(out.shown[f] - std::max(parted[0], parted[1]), out.shown[f] * 0.1f);
	return out;
}

Coefficients coefficients(const Surface &s, float alpha)
{
	const float slope = 2 * PI_F * s.aspect / (s.aspect + 2);
	const float flow = alpha + s.incidence + 2 * s.camber;
	// a thin plate lets go early, a rounder one holds on longer
	const float stall = (10 + 6 * std::min(s.thickness / 0.15f, 1.f)) * PI_F / 180;
	const float gone = smoothstep(stall, stall + 10 * PI_F / 180, std::abs(flow));
	// the air parted round a plate: flat on, pushed along its normal
	const float plate = 2 * std::sin(flow) * std::abs(std::cos(flow));
	const float blunt = 1 - smoothstep(0.3f, 0.6f, s.thickness);
	const float lift = ((1 - gone) * slope * flow + gone * plate) * blunt;
	// its friction, and more the thicker it is: what's in the way of the air (aeroOf)
	const float friction = 0.01f + 0.1f * std::min(s.thickness, 0.6f);
	return { lift, friction + lift * lift / (PI_F * 0.8f * std::max(s.aspect, 0.1f)) };
}

bool centreOfLift(const std::vector<Aero> &bodies, v3f up, v3f &centre)
{
	float weight = 0;
	v3f sum(0);
	for (const Aero &a : bodies)
		for (const Surface &s : a.surfaces) {
			// what's on its side answers a change of pitch not at all
			const float level = std::abs(s.normal.dotProduct(up));
			const float answers = s.area * level * s.aspect / (s.aspect + 2);
			sum += s.at * answers;
			weight += answers;
		}
	if (weight <= 0)
		return false;
	centre = sum / weight;
	return true;
}

} // namespace physics
