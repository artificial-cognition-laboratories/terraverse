// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "structure.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace physics {

namespace {

// Stiffness only shares the load out, so one material's will do: what a joint
// carries in a statically indeterminate frame goes by its shape far more.
constexpr double YOUNG = 1e9, SHEAR = 0.4e9;
constexpr int MOST_ROUNDS = 64;
constexpr int MOST_ITERATIONS = 20000;
constexpr double CONVERGED = 1e-3; // of the load's size: a rough answer's all this is
// Every joint this near the worst goes with it: a neck's joints fail together,
// not one per solve.
constexpr double TOGETHER = 0.5;

using V = std::array<double, 3>;
struct Dof
{
	V u, t; // displacement, rotation
};

V cross(const V &a, const V &b)
{
	return { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
}

// A joint's springs: its forces and moments in each axis.
struct Springs
{
	V k, r; // force per metre, moment per radian
};

Springs springsOf(const StructureBond &b)
{
	Springs s;
	const int p = (b.axis + 1) % 3, q = (b.axis + 2) % 3;
	const double shear = SHEAR * b.area / b.length;
	s.k[b.axis] = YOUNG * b.area / b.length;
	s.k[p] = s.k[q] = shear;
	// (the bend array is about the other two axes in order: X, Y, Z without its own)
	const int lo = std::min(p, q), hi = std::max(p, q);
	s.r[lo] = YOUNG * b.bend[0] / b.length;
	s.r[hi] = YOUNG * b.bend[1] / b.length;
	s.r[b.axis] = SHEAR * (b.bend[0] + b.bend[1]) / b.length;
	return s;
}

// The force and moment on a from b across the joint, for how they've moved.
void across(const StructureBond &b, const Springs &s, const Dof &da, const Dof &db, V &force, V &moment)
{
	// the face's middle, seen from each cell
	V ra{ 0, 0, 0 }, rb{ 0, 0, 0 };
	ra[b.axis] = b.length / 2;
	rb[b.axis] = -b.length / 2;
	const V ca = cross(da.t, ra), cb = cross(db.t, rb);
	for (int i = 0; i < 3; i++) {
		force[i] = s.k[i] * ((db.u[i] + cb[i]) - (da.u[i] + ca[i]));
		moment[i] = s.r[i] * (db.t[i] - da.t[i]);
	}
}

// K x: every joint's forces on its cells; held cells don't move and feel nothing.
void multiply(const Structure &st, const std::vector<Springs> &springs, const std::vector<char> &live,
		const std::vector<char> &alive, const std::vector<Dof> &x, std::vector<Dof> &out)
{
	std::fill(out.begin(), out.end(), Dof{});
	for (size_t i = 0; i < st.bonds.size(); i++) {
		if (!live[i])
			continue;
		const StructureBond &b = st.bonds[i];
		V f, m;
		across(b, springs[i], x[b.a], x[b.b], f, m);
		V ra{ 0, 0, 0 }, rb{ 0, 0, 0 };
		ra[b.axis] = b.length / 2;
		rb[b.axis] = -b.length / 2;
		const V ma = cross(ra, f), mb = cross(rb, f);
		for (int k = 0; k < 3; k++) {
			out[b.a].u[k] -= f[k];
			out[b.a].t[k] -= ma[k] + m[k];
			out[b.b].u[k] += f[k];
			out[b.b].t[k] += mb[k] + m[k];
		}
	}
	for (size_t i = 0; i < out.size(); i++)
		if (!alive[i])
			out[i] = Dof{};
}

double dot(const std::vector<Dof> &a, const std::vector<Dof> &b)
{
	double sum = 0;
	for (size_t i = 0; i < a.size(); i++)
		for (int k = 0; k < 3; k++)
			sum += a[i].u[k] * b[i].u[k] + a[i].t[k] * b[i].t[k];
	return sum;
}

// How connected everything is through the joints still there: each cell's label.
std::vector<unsigned> components(const Structure &st, const std::vector<char> &live)
{
	std::vector<unsigned> up(st.cells.size());
	std::iota(up.begin(), up.end(), 0u);
	const auto find = [&](unsigned x) {
		while (up[x] != x)
			x = up[x] = up[up[x]];
		return x;
	};
	for (size_t i = 0; i < st.bonds.size(); i++)
		if (live[i])
			up[find(st.bonds[i].a)] = find(st.bonds[i].b);
	for (unsigned i = 0; i < up.size(); i++)
		up[i] = find(i);
	return up;
}

} // namespace

StructureResult solveStructure(const Structure &st, const std::vector<bool> &resting, const StructureLoad &pull)
{
	if (resting.size() != st.cells.size())
		throw std::invalid_argument("solveStructure: a resting flag per cell");
	for (const StructureBond &b : st.bonds)
		if (b.a >= st.cells.size() || b.b >= st.cells.size() || b.axis > 2 || b.area <= 0 || b.length <= 0)
			throw std::invalid_argument("solveStructure: a joint that isn't one");

	StructureResult result;
	std::vector<char> live(st.bonds.size(), 1);
	std::vector<Springs> springs;
	springs.reserve(st.bonds.size());
	for (const StructureBond &b : st.bonds)
		springs.push_back(springsOf(b));
	const size_t n = st.cells.size();
	const V g{ pull.acceleration.X, pull.acceleration.Y, pull.acceleration.Z };
	const V spin{ pull.spin.X, pull.spin.Y, pull.spin.Z };
	V middle{ 0, 0, 0 };
	double total = 0;
	for (const StructureCell &c : st.cells) {
		middle[0] += c.mass * c.centre.X;
		middle[1] += c.mass * c.centre.Y;
		middle[2] += c.mass * c.centre.Z;
		total += c.mass;
	}
	for (double &m : middle)
		m /= std::max(total, 1e-9);

	// A diagonal to precondition by: a cell's own springs.
	const auto diagonal = [&]() {
		std::vector<Dof> d(n);
		for (size_t i = 0; i < st.bonds.size(); i++) {
			if (!live[i])
				continue;
			const StructureBond &b = st.bonds[i];
			const double half = b.length / 2;
			for (const unsigned c : { b.a, b.b })
				for (int k = 0; k < 3; k++) {
					d[c].u[k] += springs[i].k[k];
					d[c].t[k] += springs[i].r[k];
					if (k != b.axis) // what turning it about k moves at the face, across
						d[c].t[k] += springs[i].k[3 - b.axis - k] * half * half;
				}
		}
		return d;
	};

	double energy = pull.energy;
	std::vector<Dof> x(n); // kept: each solve starts from the last, a few joints different
	for (result.rounds = 0; (int)result.rounds < MOST_ROUNDS;) {
		// Only what still reaches something holding it is carried; the rest is falling.
		const std::vector<unsigned> label = components(st, live);
		std::vector<char> held(n, 0);
		for (size_t i = 0; i < n; i++)
			if (resting[i])
				held[label[i]] = 1;
		std::vector<char> alive(n, 0), joints(st.bonds.size(), 0);
		std::vector<Dof> f(n);
		double size = 0;
		for (size_t i = 0; i < n; i++) {
			alive[i] = held[label[i]] && !resting[i];
			if (!alive[i])
				continue;
			const V arm{ st.cells[i].centre.X - middle[0], st.cells[i].centre.Y - middle[1], st.cells[i].centre.Z - middle[2] };
			const V turning = cross(spin, arm);
			for (int k = 0; k < 3; k++) {
				f[i].u[k] = st.cells[i].mass * (g[k] + turning[k]);
				if (!pull.each.empty())
					f[i].u[k] += st.cells[i].mass * (k == 0 ? pull.each[i].X : k == 1 ? pull.each[i].Y : pull.each[i].Z);
				size += f[i].u[k] * f[i].u[k];
			}
		}
		for (size_t i = 0; i < st.bonds.size(); i++)
			joints[i] = live[i] && held[label[st.bonds[i].a]];
		if (size == 0)
			break; // nothing's hanging from anything

		result.rounds++;
		const std::vector<Dof> diag = diagonal();
		const auto precondition = [&](const std::vector<Dof> &r, std::vector<Dof> &z) {
			for (size_t i = 0; i < n; i++)
				for (int k = 0; k < 3; k++) {
					z[i].u[k] = alive[i] && diag[i].u[k] > 0 ? r[i].u[k] / diag[i].u[k] : 0;
					z[i].t[k] = alive[i] && diag[i].t[k] > 0 ? r[i].t[k] / diag[i].t[k] : 0;
				}
		};
		// conjugate gradients: K is symmetric, and positive with the held cells out
		std::vector<Dof> r(n), z(n), p(n), kp(n);
		for (size_t i = 0; i < n; i++)
			if (!alive[i])
				x[i] = Dof{}; // fallen off, or held
		multiply(st, springs, joints, alive, x, kp);
		for (size_t i = 0; i < n; i++)
			for (int k = 0; k < 3; k++) {
				r[i].u[k] = f[i].u[k] - kp[i].u[k];
				r[i].t[k] = f[i].t[k] - kp[i].t[k];
			}
		precondition(r, z);
		p = z;
		double rz = dot(r, z);
		const double target = CONVERGED * CONVERGED * size;
		bool converged = dot(r, r) < target;
		for (int it = 0; !converged && it < MOST_ITERATIONS; it++) {
			multiply(st, springs, joints, alive, p, kp);
			const double pkp = dot(p, kp);
			if (pkp <= 0)
				break;
			const double alpha = rz / pkp;
			for (size_t i = 0; i < n; i++)
				for (int k = 0; k < 3; k++) {
					x[i].u[k] += alpha * p[i].u[k];
					x[i].t[k] += alpha * p[i].t[k];
					r[i].u[k] -= alpha * kp[i].u[k];
					r[i].t[k] -= alpha * kp[i].t[k];
				}
			if (dot(r, r) < target) {
				converged = true;
				break;
			}
			precondition(r, z);
			const double next = dot(r, z);
			const double beta = next / rz;
			rz = next;
			for (size_t i = 0; i < n; i++)
				for (int k = 0; k < 3; k++) {
					p[i].u[k] = z[i].u[k] + beta * p[i].u[k];
					p[i].t[k] = z[i].t[k] + beta * p[i].t[k];
				}
		}
		if (!converged) {
			result.settled = false;
			break;
		}

		// each joint's load against what it can take
		std::vector<double> load(st.bonds.size(), 0);
		double worst = 0;
		for (size_t i = 0; i < st.bonds.size(); i++) {
			if (!joints[i])
				continue;
			const StructureBond &b = st.bonds[i];
			const StructureCell &ca = st.cells[b.a], &cb = st.cells[b.b];
			const double tension = std::min(ca.tension > 0 ? ca.tension : INFINITY, cb.tension > 0 ? cb.tension : INFINITY);
			const double compression = std::min(ca.compression > 0 ? ca.compression : INFINITY, cb.compression > 0 ? cb.compression : INFINITY);
			if (std::isinf(tension) && std::isinf(compression))
				continue;
			V force, moment;
			across(b, springs[i], x[b.a], x[b.b], force, moment);
			const int pa = (b.axis + 1) % 3, qa = (b.axis + 2) % 3;
			const int lo = std::min(pa, qa), hi = std::max(pa, qa);
			// bending about lo stretches fibres along hi, and so on (the reaches are by their own axis)
			const double fibres = (b.bend[0] > 0 ? std::fabs(moment[lo]) * b.reach[1] / b.bend[0] : 0)
					+ (b.bend[1] > 0 ? std::fabs(moment[hi]) * b.reach[0] / b.bend[1] : 0);
			const double axial = force[b.axis] / b.area;
			const double sheared = 1.5 * std::hypot(force[pa], force[qa]) / b.area
					+ std::fabs(moment[b.axis]) * std::max(b.reach[0], b.reach[1]) / std::max(1e-12f, b.bend[0] + b.bend[1]);
			load[i] = std::max({ (axial + fibres) / tension, (fibres - axial) / compression, sheared / tension });
			worst = std::max(worst, load[i]);
		}
		if (result.rounds == 1)
			result.worst = (float)worst;
		if (worst <= 1)
			break;
		// hardest hit first, while the blow has the energy for the crack
		std::vector<size_t> over;
		for (size_t i = 0; i < st.bonds.size(); i++)
			if (load[i] > 1 && load[i] >= worst * TOGETHER)
				over.push_back(i);
		std::sort(over.begin(), over.end(), [&](size_t a, size_t b) { return load[a] > load[b]; });
		bool spent = false;
		for (const size_t i : over) {
			const StructureBond &b = st.bonds[i];
			const double cost = b.area * std::max(st.cells[b.a].toughness, st.cells[b.b].toughness);
			if (pull.energy > 0 && energy < cost) {
				spent = true;
				break;
			}
			energy -= cost;
			live[i] = 0;
			result.broken++;
		}
		if (spent)
			break;
	}

	if (!result.settled) { // no solve's word to go on: it stays as it was built
		std::fill(live.begin(), live.end(), 1);
		result.broken = 0;
	}

	// the pieces, largest first
	const std::vector<unsigned> label = components(st, live);
	std::vector<double> mass(n, 0);
	for (size_t i = 0; i < n; i++)
		mass[label[i]] += st.cells[i].mass;
	std::vector<unsigned> order;
	for (unsigned i = 0; i < n; i++)
		if (label[i] == i)
			order.push_back(i);
	std::stable_sort(order.begin(), order.end(), [&](unsigned a, unsigned b) { return mass[a] > mass[b]; });
	std::vector<unsigned> index(n, 0);
	for (unsigned i = 0; i < order.size(); i++)
		index[order[i]] = i;
	result.group.resize(n);
	for (size_t i = 0; i < n; i++)
		result.group[i] = index[label[i]];
	result.groups = (unsigned)order.size();
	return result;
}

} // namespace physics
