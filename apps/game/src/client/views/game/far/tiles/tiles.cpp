// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/game/far/tiles/tiles.h"
#include "src/client/views/game/far/land/mesh.h"
#include "src/client/views/game/far/tiles/workers.h"
#include "src/client/views/game/far/tiles/preview.h"
#include "src/shared/base/constants.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/platform/system/log.h"
#include "library/world/terragen/src/base/progress.h"
#include "library/trace/owned.h"
#include <atomic>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include "library/trace/trace.h"
#include <IVideoDriver.h>
#include "library/world/terragen/src/base/parallel.h"

namespace far {

// What the held tiles' meshes may take, all of them: past it the least
// recently wanted go. (Dropped after ten seconds unwanted, a spot panned away
// from and back to was built again, coarse meanwhile; sprint 114). At 256 MB the
// Planetarium's whole planet from orbit was over it alone: zoomed in, every
// one went, and zooming out built the planet again (bun perf:planetarium)
static constexpr size_t KEEP_BYTES = (size_t)512 << 20;
static constexpr u32 KEEP_UPDATES = 120; // a tile wanted this lately isn't evicted (about two seconds)
// Tiles this few levels under their roots are never evicted: what the whole
// planet's drawn with from afar (the Planetarium's from orbit, the far view's
// horizon), a few hundred MB at most. Evicted, zooming out took them all
// again, the planet laid anew (bun perf:planetarium); only fine tiles left
// behind go.
static constexpr s32 KEEP_COARSE = 3;
// A tile drawn whole this near its split (its split distance times this) has
// its children built ahead while the eye's coming down (Eye::ahead), so
// zooming in finds them there, as a globe viewer loads the next level ahead
// of the camera. Built ahead always, a third of the view's tiles asked for
// four children each: the workers never idle, and twice the memory
static constexpr float AHEAD = 1.25f;
static constexpr float STAY_WHOLE = 0.85f;
// How many levels finer a tile not built yet looks for finer ones to stand in for it.
static constexpr s32 FINER_DEPTH = 3;

// Each Tiles its own figure: the game's far view and the planetarium's may show one body at once.
static std::string ownerName(std::string_view system, int body)
{
	static std::atomic<u32> made{ 0 };
	return std::string(system) + ": tiles #" + std::to_string(++made) + " (body " + std::to_string(body) + ")";
}

unsigned Tiles::threadsFor(float share)
{
	const unsigned cores = terragen::cores();
	return std::max(1u, (unsigned)(cores * share));
}

Tiles::Tiles(const Opts &o) :
	m_atlas(o.atlas),
	m_horizon(o.horizon),
	m_tiling(o.atlas.body(o.body)),
	m_workers(std::make_unique<Workers>(m_atlas, o.body, m_horizon, o.palette, m_tiling, o.threads)),
	m_coarse(std::make_unique<terragen::Step>(o.horizon.code, "coarse planet", (int64_t)m_tiling.roots().size())),
	m_driver(o.driver),
	m_own(o.budget ? nullptr : std::make_unique<Budget>()),
	m_budget(o.budget ? *o.budget : *m_own),
	m_owner(ownerName(o.system, o.body)),
	m_paint(o.paint)
{
}

Tiles::~Tiles()
{
	trace::disowned(m_owner);
	trace::disowned(m_owner + " on the card");
}

void Tiles::update(const Eye &eye)
{
	if (m_own)
		m_own->frame();
	// Choosing walks the tree from every root, each tile weighed for distance
	// and sight: most of the far view's time each frame, standing still too,
	// to choose what it chose before. Only when the eye's moved, or a tile's
	// come or gone; otherwise only finished tiles are taken.
	// Standing still while tiles come in, every few frames is soon enough to
	// put them in: each install changed it, and the tree was walked each frame.
	// Moving, every frame (a turn waiting a frame left holes at the edge).
	constexpr u32 SETTLING = 4;
	// first: a tile the driver let go of is dropped, and what's drawn chosen again without it
	const bool lost = touch();
	const bool moved = !still(eye);
	if (lost || m_rechoose || moved || (m_changed && ++m_settling >= SETTLING)) {
		m_rechoose = false;
		m_settling = 0;
		choose(eye);
	}
	{
		TF_ZONE("Far view: install");
		m_changed |= install();
	}
	if (m_coarse && based()) {
		m_coarse.reset();
		actionstream << "far: " << m_horizon.code << " coarse planet there; " << terragen::breakdown(terragen::making(m_horizon.code)) << std::endl;
	}
	{
		TF_ZONE("Far view: repaint");
		m_changed |= m_swept != m_tints;
		repaint();
	}
	m_stats.now(m_tiles.size(), m_bytes, m_pending, m_draw.size(), m_holes, m_unready);
	trace::owned(m_owner, m_ram, m_tiles.size());
	trace::owned(m_owner + " on the card", m_bytes - m_ram, m_tiles.size(), trace::Where::Gpu);
}

void Tiles::choose(const Eye &eye)
{
	m_frame++;
	m_holes = 0;
	m_unready = 0;
	m_ahead.clear();
	m_draw.clear();
	m_behind.clear();
	std::vector<std::pair<float, Key>> want;
	{
		TF_ZONE("Far view: choose");
		for (const TileKey &root : m_tiling.roots())
			choose(Key(root), eye, m_draw, want);
	}

	// the roots, everywhere, before anything: a coarse planet always there to
	// stand in while the eye's own tiles are built (turning never opens holes).
	// Sketched first, all of them, so the whole planet shows in a fraction of
	// the time; then built properly
	TF_ZONE("Far view: roots");
	std::set<Key> sketches;
	for (const TileKey &root : m_tiling.roots()) {
		Entry &e = m_tiles[Key(root)];
		e.used = m_frame;
		if (e.ready && !e.sketch)
			continue;
		const float first = e.ready ? -1.0f : -2.0f;
		if (!e.ready)
			sketches.insert(Key(root));
		auto again = std::find_if(want.begin(), want.end(), [&](const auto &w) { return !(w.second < Key(root)) && !(Key(root) < w.second); });
		if (again != want.end())
			again->first = first; // wanted already: first, now
		else
			want.emplace_back(first, Key(root));
	}
	m_pending = want.size(); // (what's asked for ahead isn't: the view's complete without it)
	if (eye.hurried) { // passing through: a coarse preview of what's wanted (tiles/preview.h)
		std::vector<std::pair<float, TileKey>> wanted;
		for (const auto &[priority, key] : want)
			wanted.emplace_back(priority, key.tile());
		want.clear();
		for (const auto &[priority, key] : preview::of(wanted, m_tiling, [&](const TileKey &k) {
				auto it = m_tiles.find(Key(k));
				return it != m_tiles.end() && it->second.ready; }))
			want.emplace_back(priority, Key(key));
	}
	std::sort(want.begin(), want.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
	std::sort(m_ahead.begin(), m_ahead.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
	for (const auto &a : m_ahead)
		want.push_back(a); // after all that's needed now
	{
		TF_ZONE("Far view: want");
		std::vector<Workers::Job> jobs;
		jobs.reserve(want.size());
		for (const auto &w : want)
			jobs.push_back({ w.second.tile(), sketches.count(w.second) > 0 });
		m_workers->want(jobs);
	}

	m_chosen_for = eye;
	m_changed = false;
	if (m_frame % 60 == 0) {
		TF_ZONE("Far view: evict");
		m_changed |= evict();
	}
}

bool Tiles::still(const Eye &eye) const
{
	if (!m_chosen_for)
		return false;
	const Eye &was = *m_chosen_for;
	constexpr double MOVED = 0.01, TURNED = 1e-5; // blocks; of a unit vector
	const auto off = [](const Tiling::Vec &a, const Tiling::Vec &b) {
		return std::max({ std::abs(a.x - b.x), std::abs(a.y - b.y), std::abs(a.z - b.z) });
	};
	return off(eye.at, was.at) < MOVED && std::abs(eye.altitude - was.altitude) < MOVED
			&& eye.cut == was.cut && eye.range == was.range && eye.split == was.split && eye.keep_fine == was.keep_fine
			&& eye.hurried == was.hurried && eye.ahead == was.ahead
			&& off(eye.heading, was.heading) < TURNED && off(eye.look, was.look) < TURNED
			&& off(eye.right, was.right) < TURNED && off(eye.upward, was.upward) < TURNED
			&& eye.wide == was.wide && eye.high == was.high;
}

float Tiles::basedShare() const
{
	const auto &roots = m_tiling.roots();
	size_t built = 0;
	for (const TileKey &root : roots) {
		auto it = m_tiles.find(Key(root));
		built += it != m_tiles.end() && it->second.ready;
	}
	return roots.empty() ? 1.0f : (float)built / roots.size();
}

bool Tiles::based() const
{
	for (const TileKey &root : m_tiling.roots()) {
		auto it = m_tiles.find(Key(root));
		if (it == m_tiles.end() || !it->second.ready)
			return false;
	}
	return true;
}

std::vector<Tiles::Drawn> Tiles::behind() const
{
	std::vector<Drawn> out;
	out.reserve(m_behind.size());
	for (const auto &[key, child] : m_behind) {
		const auto it = m_tiles.find(key);
		if (it == m_tiles.end())
			continue; // dropped since it was chosen (tint()): the next update chooses again
		out.push_back({ key.tile(), place(key).middle, it->second.quarters[child], key.child(child).tile() });
	}
	return out;
}

std::vector<Tiles::Drawn> Tiles::drawn() const
{
	std::vector<Drawn> out;
	out.reserve(m_draw.size());
	for (const Key &key : m_draw) {
		const auto it = m_tiles.find(key);
		if (it == m_tiles.end())
			continue; // dropped since it was chosen (tint()): the next update chooses again
		out.push_back({ key.tile(), place(key).middle, it->second.parts });
	}
	return out;
}

float Tiles::distance(const TileKey &key, const Eye &eye) const
{
	const Place &at = place(Key(key));
	const Tiling::Vec &p = at.middle;
	const double r = m_tiling.radius(), sea = r + m_horizon.water_level + 1;
	const double d = std::hypot(p.x * sea / r - eye.at.x, p.y * sea / r - eye.at.y, p.z * sea / r - eye.at.z);
	return (float)std::max(d - at.half, 0.0);
}

const Tiles::Place &Tiles::place(const Key &key) const
{
	auto it = m_places.find(key);
	if (it != m_places.end())
		return it->second;
	// (a long journey's worth, then started again)
	if (m_places.size() > 1 << 18)
		m_places.clear();
	return m_places.emplace(key, Place{ m_tiling.middle(key.tile()), m_tiling.diagonal(key.tile()) / 2 }).first->second;
}

bool Tiles::seen(const TileKey &key, const Eye &eye) const
{
	if (eye.wide <= 0)
		return true;
	// The tile as a ball round its middle, half its diagonal wide, at the sea
	// and RELIEF over it (its land is between), against the four sides of
	// what the eye sees: a side through the eye, square to n, inside where
	// v · n ≤ 0 (v from the eye). A circle round the screen took in its
	// corners' worth all round it
	const double r = std::hypot(eye.at.x, eye.at.y, eye.at.z), up = (r + eye.altitude) / r;
	const Tiling::Vec &m = place(Key(key)).middle;
	const double mr = std::hypot(m.x, m.y, m.z), half = place(Key(key)).half;
	const auto dot = [](const Tiling::Vec &a, double x, double y, double z) { return a.x * x + a.y * y + a.z * z; };
	for (const double h : { 0.0, (double)RELIEF }) {
		const double k = (mr + h) / mr;
		const double vx = m.x * k - eye.at.x * up, vy = m.y * k - eye.at.y * up, vz = m.z * k - eye.at.z * up;
		const double ahead = dot(eye.look, vx, vy, vz), across = dot(eye.right, vx, vy, vz), above = dot(eye.upward, vx, vy, vz);
		// each side's (v · n) · |n|, n = (±side − look · tan); |n| = √(1 + tan²)
		const double nw = std::sqrt(1 + (double)eye.wide * eye.wide), nh = std::sqrt(1 + (double)eye.high * eye.high);
		if (std::abs(across) - ahead * eye.wide <= half * nw && std::abs(above) - ahead * eye.high <= half * nh)
			return true;
	}
	return false;
}

bool Tiles::choose(const Key &key, const Eye &eye, std::vector<Key> &draw, std::vector<std::pair<float, Key>> &want)
{
	if (!m_tiling.within(key.tile()))
		return true; // past its region's grid: nothing there
	float d = distance(key.tile(), eye);
	if (d > eye.range)
		return true; // beyond the eye's reach: nothing to draw
	if (!seen(key.tile(), eye))
		return true; // off screen: nothing to draw (drawn, a coarse planet's worth of it went to the GPU each frame)
	// Within the near world's range as fine as at its edge: what it hasn't loaded
	// yet shows this, a column at a time (far/mask.h), and must be the ground's
	// own shape, not the coarse planet's cells.
	// high up, a tile's size on screen goes by how far it is through the air
	float seen = std::sqrt(d * d + eye.altitude * eye.altitude);
	// (once whole a tile stays whole a little longer: on the line, it flipped between two details)
	const auto held = m_tiles.find(key);
	const float stay = held != m_tiles.end() && held->second.whole == m_frame - 1 ? STAY_WHOLE : 1.0f;
	bool split = key.level > 0 && seen - eye.cut < (float)key.tile().span() * eye.split * stay;

	if (!split) {
		Entry &e = m_tiles[key];
		e.used = m_frame;
		if (e.ready) {
			e.whole = m_frame;
			draw.push_back(key);
			// near splitting: its children asked for ahead, after all that's needed
			if (eye.ahead && key.level > 0 && seen - eye.cut < (float)key.tile().span() * eye.split * AHEAD)
				for (s32 c = 0; c < 4; c++) {
					const Key child = key.child(c);
					auto ce = m_tiles.find(child);
					if ((ce == m_tiles.end() || !ce->second.ready) && Tiles::seen(child.tile(), eye))
						m_ahead.emplace_back(d / (float)child.tile().span(), child);
				}
			return true;
		}
		{
			const Tiling::Vec &m = place(key).middle;
			const double to[3] = { m.x - eye.at.x, m.y - eye.at.y, m.z - eye.at.z };
			const double len = std::max(1.0, std::hypot(to[0], to[1], to[2]));
			const double ahead = (to[0] * eye.heading.x + to[1] * eye.heading.y + to[2] * eye.heading.z) / len;
			// by distance in its own spans, so coarse tiles come before fine ones
			// at the same place: a missing tile's stand-in is then its parent,
			// not an ancestor kilometres wide (flying fast, hundreds wanted at
			// once, the flat planes that flickered were those ancestors)
			want.emplace_back(d * (float)(1 - 0.6 * ahead) / (float)key.tile().span(), key);
		}
		// meanwhile, finer tiles from before, if all of them are there
		size_t mark = draw.size();
		// (standing in behind them, a coarser tile needn't have all of them:
		// what's there is kept, and it shows only through the rest)
		// (no deeper than FINER_DEPTH: kept looking past a missing one, with
		// keep_fine, a coarse planet not there yet was a search of every tile
		// under it to the finest, millions a frame; sprint 114)
		std::function<bool(const Key &, s32)> finer = [&](const Key &k, s32 depth) {
			if (k.level == 0 || depth == FINER_DEPTH)
				return false;
			bool all = true;
			for (s32 c = 0; c < 4 && (all || eye.keep_fine); c++) {
				const Key child = k.child(c);
				auto it = m_tiles.find(child);
				if (it != m_tiles.end() && it->second.ready) {
					it->second.used = m_frame;
					draw.push_back(child);
				} else if (!finer(child, depth + 1)) {
					all = false;
				}
			}
			return all;
		};
		if (finer(key, 0))
			return true;
		if (!eye.keep_fine)
			draw.resize(mark);
		// with keep_fine, the parent stands in for it only if it isn't too coarse
		// to: else it's a hole, and no coarser tile's asked to fill it
		if (eye.keep_fine && !fits(key.tile().span() * 2, key, eye)) {
			m_holes++;
			return true;
		}
		return false;
	}

	size_t mark = draw.size();
	bool all = true, there[4];
	for (s32 c = 0; c < 4; c++)
		all &= there[c] = choose(key.child(c), eye, draw, want);
	if (all)
		return true;
	// a finer tile isn't there yet: this one stands in for all four
	auto it = m_tiles.find(key);
	if (it != m_tiles.end() && it->second.ready && eye.keep_fine) {
		// behind the finer ones that are there, so only the hole shows it: swapped
		// for them, a whole view flashed coarse for each new tile. Only its
		// quarters over the holes: the rest of it, at its own coarser heights,
		// showed past the finer land's edge, a block of it above or below the ground.
		// A quarter too coarse for its hole is left out: the hole stays open
		it->second.used = m_frame;
		for (s32 c = 0; c < 4; c++) {
			if (there[c])
				continue;
			if (fits(key.tile().span(), key.child(c), eye)) {
				m_behind.emplace_back(key, c);
				const Entry::Range &r = it->second.ranges[c];
				m_unready += it->second.quarters[c].empty() && r.v1 > r.v0; // land there, nothing to draw it with: a gap
			} else {
				m_holes++;
			}
		}
		return true;
	}
	if (eye.keep_fine && !fits(key.tile().span() * 2, key, eye)) {
		for (s32 c = 0; c < 4; c++)
			m_holes += !there[c];
		return true; // its parent's too coarse to stand in: the holes stay open
	}
	if (it != m_tiles.end() && it->second.ready) {
		draw.resize(mark);
		draw.push_back(key);
		it->second.used = m_frame;
		return true;
	}
	return false;
}

bool Tiles::fits(s32 span, const Key &hole, const Eye &eye) const
{
	// the span the eye would draw at the hole's distance: a tile splits while
	// nearer than `split` of its spans past the cut (choose), so about this
	const float d = distance(hole.tile(), eye);
	const float seen = std::sqrt(d * d + eye.altitude * eye.altitude);
	const float wanted = std::max((seen - eye.cut) / eye.split, (float)TILE);
	return eye.stand_in <= 0 || (float)span <= eye.stand_in * wanted;
}

bool Tiles::install()
{
	bool took = false;
	while (m_budget.room(Budget::Work::Install)) {
		std::optional<Workers::Result> result = m_workers->take();
		if (!result)
			return took;
		took = true;
		m_budget.run(Budget::Work::Install, [&] { take(result->key, result->mesh, result->sketch); });
	}
	return took;
}

void Tiles::take(const TileKey &key, Packed &packed, bool sketch)
{
	Entry &e = m_tiles[Key(key)];
	m_bytes -= e.bytes;
	m_ram -= e.ram;
	e.bytes = 0;
	e.ram = 0;
	e.parts.clear();
	for (auto &q : e.quarters)
		q.clear();
	e.merged = packed.merged;
	e.own = std::move(packed.own);
	for (u8 q = 0; q < 4; q++)
		e.ranges[q] = { packed.ranges[q].v0, packed.ranges[q].v1, packed.ranges[q].i0, packed.ranges[q].i1 };
	for (size_t p = 0; p < packed.parts.size(); p++) {
		Packed::Part &part = packed.parts[p];
		auto buffer = make_irr<TileBuffer>();
		e.bytes += part.vertices.size() * sizeof(PackedVertex) + part.indices.size() * sizeof(u16);
		buffer->Vertices->Data = std::move(part.vertices);
		buffer->Indices->Data = std::move(part.indices);
		buffer->setBoundingBox(packed.boxes[p]); // (found by the worker)
		buffer->setHardwareMappingHint(scene::EHM_STATIC);
		if (!e.merged)
			e.quarters[part.quarter].push_back(buffer);
		e.parts.push_back(std::move(buffer));
	}
	if (e.merged)
		quarter(e, packed.quarterBoxes);
	m_bytes += e.bytes;
	e.ram = e.bytes;
	m_ram += e.ram;
	m_stats.taken(key, sketch, e.bytes);
	if (m_tint)
		paint(Key(key), e);
	e.painted = m_tints;
	// on the GPU now, in the install's time, painted: not as it's first
	// drawn, where a burst of new tiles was tens of ms in one frame's draw
	for (const auto &part : e.parts)
		upload(*part);
	for (const auto &q : e.quarters)
		for (const auto &part : q)
			upload(*part); // (their indices: the vertices went with the tile's)
	// on the card alone, unless it may be repainted in place: a tinted Tiles' coarse ones
	// (few, and what stands in while the rest are built again painted: tint())
	if (m_paint == Paint::Never || m_tiling.top(key.face) - key.level >= KEEP_COARSE)
		release(e);
	if (m_coarse && !e.ready && key.level == m_tiling.top(key.face))
		m_coarse->advance();
	e.ready = true;
	e.sketch = sketch;
}

// The stand-ins behind finer tiles cut from their tiles, as many as the frame
// has time for: one missing a frame or two leaves a gap it would only have
// stood behind (a tint's repaint unmade them all at once: 16 ms of copying
// and uploading in one frame)
void Tiles::quarter(Entry &e, const std::array<aabb3f, 4> &boxes)
{
	// Each quarter its own indices over the tile's own vertices, shared: made
	// as the tile's taken, so a tile split before its children are built
	// stands in behind them at once. Made when first wanted instead (each a
	// copy of its vertices, uploaded, a few a frame), a zoom across a split
	// wanted hundreds in a frame, and each not made yet was a gap to space.
	const TileBuffer &all = *e.parts.front();
	for (s32 q = 0; q < 4; q++) {
		const Entry::Range &r = e.ranges[q];
		if (r.v1 == r.v0)
			continue; // nothing of it there
		auto buffer = make_irr<TileBuffer>();
		buffer->Vertices->drop();
		buffer->Vertices = all.Vertices; // (repainted with the tile: they're its own)
		buffer->Vertices->grab();
		buffer->Indices->Data.assign(all.Indices->Data.begin() + r.i0, all.Indices->Data.begin() + r.i1);
		buffer->setBoundingBox(boxes[q]);
		buffer->setHardwareMappingHint(scene::EHM_STATIC);
		e.bytes += buffer->Indices->Data.size() * sizeof(u16);
		e.quarters[q].push_back(std::move(buffer));
	}
}

void Tiles::tint(std::shared_ptr<const Tint> tint)
{
	if (m_paint == Paint::Never && tint)
		throw BaseException("far: " + m_owner + " lets its meshes go once they're on the card, so it can't be tinted (Paint::Tinted)");
	if (!tint && !m_tint)
		return; // their own colours still: nothing to repaint (each screen the menu went to set none again, every tile repainted and sent to the GPU again)
	m_tint = std::move(tint);
	m_tints++; // every tile out of date: update() sweeps over them
	// what's on the card alone can't be repainted: let go of, and built again (from the
	// disk cache) painted as it's taken; the coarse ones, kept, are repainted and stand in
	for (auto it = m_tiles.begin(); it != m_tiles.end();) {
		Entry &e = it->second;
		if (e.parts.empty() || !e.parts.front()->Vertices->released()) {
			++it;
			continue;
		}
		m_bytes -= e.bytes;
		m_ram -= e.ram;
		it = m_tiles.erase(it);
		m_rechoose = true;
	}
}

void Tiles::trim()
{
	size_t gone = 0;
	for (auto it = m_tiles.begin(); it != m_tiles.end();) {
		const Entry &e = it->second;
		if (e.used == m_frame || m_tiling.top(it->first.face) - it->first.level < KEEP_COARSE) {
			++it;
			continue;
		}
		m_bytes -= e.bytes;
		m_ram -= e.ram;
		it = m_tiles.erase(it);
		gone++;
	}
	m_stats.evicted(gone);
	if (gone)
		m_rechoose = true; // a stand-in drawn may be gone: chosen again before it's drawn
}

void Tiles::repaint()
{
	if (m_swept == m_tints)
		return; // (looking over every tile and the clock each, each frame, cost the Planetarium's frames)
	const auto again = [&](const Key &key, Entry &e) {
		if (!e.ready || e.painted == m_tints)
			return true;
		if (!m_budget.room(Budget::Work::Paint))
			return false;
		m_budget.run(Budget::Work::Paint, [&] {
			paint(key, e);
			for (const auto &part : e.parts)
				upload(*part);
		});
		e.painted = m_tints;
		return true;
	};
	// the drawn first: what's on screen changes first
	for (const Key &key : m_draw)
		if (!again(key, m_tiles.at(key)))
			return;
	for (auto &[key, e] : m_tiles)
		if (!again(key, e))
			return;
	m_swept = m_tints; // (a tile installed since is painted as it's installed)
}

size_t Tiles::unpainted() const
{
	size_t n = 0;
	for (const auto &[key, e] : m_tiles)
		n += e.ready && e.painted != m_tints;
	return n;
}

void Tiles::paint(const Key &key, Entry &e) const
{
	const Tiling::Vec mid = place(key).middle;
	for (size_t p = 0; p < e.parts.size(); p++) {
		auto &vertices = e.parts[p]->Vertices->Data;
		const std::vector<video::SColor> &own = e.own.at(p);
		for (size_t i = 0; i < vertices.size(); i++) {
			PackedVertex &v = vertices[i];
			if (!m_tint) {
				v.Color = own[i];
				continue;
			}
			const Tiling::Vec at{ mid.x + v.Pos.X / BS, mid.y + v.Pos.Y / BS, mid.z + v.Pos.Z / BS };
			video::SColor c = (*m_tint)(own[i], at);
			c.setAlpha(own[i].getAlpha()); // (the alpha marks a skirt: far/mesh.cpp)
			v.Color = c;
		}
		e.parts[p]->setDirty(scene::EBT_VERTEX);
	}
}

void Tiles::release(Entry &e)
{
	if (!m_driver)
		return; // nothing's on the card till it's drawn: kept
	// each buffer the driver holds (a small one it draws from RAM: kept)
	const auto let = [](TileBuffer &b) {
		if (b.Vertices->Link)
			b.Vertices->release();
		if (b.Indices->Link)
			b.Indices->release();
	};
	for (const auto &part : e.parts)
		let(*part);
	for (const auto &q : e.quarters)
		for (const auto &part : q)
			let(*part);
	std::vector<std::vector<video::SColor>>().swap(e.own); // (only a repaint reads them)
	// what's left in RAM: the buffers the driver didn't take
	size_t kept = 0;
	for (const auto &part : e.parts)
		kept += part->Vertices->Data.size() * sizeof(PackedVertex) + part->Indices->Data.size() * sizeof(u16);
	for (const auto &q : e.quarters)
		for (const auto &part : q)
			kept += part->Indices->Data.size() * sizeof(u16);
	m_ram -= e.ram;
	e.ram = kept;
	m_ram += e.ram;
}

bool Tiles::touch()
{
	if (!m_driver)
		return false;
	// the driver lets a buffer go after 25 s unused (CNullDriver::expireHardwareBuffers):
	// touched every 5. By the clock, not by updates: the sky updates a body's tiles
	// only while it's drawn, so one out of sight went untouched past the driver's limit
	const auto now = std::chrono::steady_clock::now();
	if (now - m_touched < std::chrono::seconds(5))
		return false;
	m_touched = now;
	TF_ZONE("Far view: touch");
	// a tile whose buffer the driver let go anyway is let go of too, and built again
	// (from the disk cache): drawn, it would be uploaded from nothing
	size_t lost = 0;
	for (auto it = m_tiles.begin(); it != m_tiles.end();) {
		Entry &e = it->second;
		bool gone = false;
		const auto use = [&](TileBuffer &b) {
			if (b.Vertices->released()) {
				if (b.Vertices->Link)
					m_driver->updateHardwareBuffer(b.Vertices); // (unchanged: only marks it used)
				else
					gone = true;
			}
			if (b.Indices->released()) {
				if (b.Indices->Link)
					m_driver->updateHardwareBuffer(b.Indices);
				else
					gone = true;
			}
		};
		for (const auto &part : e.parts)
			use(*part);
		for (const auto &q : e.quarters)
			for (const auto &part : q)
				use(*part);
		if (!gone) {
			++it;
			continue;
		}
		m_bytes -= e.bytes;
		m_ram -= e.ram;
		it = m_tiles.erase(it);
		lost++;
	}
	// (the sky's bodies: one out of sight for long is let go of so, and read from the disk cache when seen again)
	if (lost)
		infostream << "far: " << m_owner << ": " << lost << " tiles unseen long enough for the driver to let them go, built again when wanted" << std::endl;
	return lost > 0;
}

void Tiles::upload(TileBuffer &buffer) const
{
	if (!m_driver)
		return; // (it goes to the GPU when it's first drawn)
	TF_ZONE("Far view: upload");
	m_driver->updateHardwareBuffer(buffer.getVertexBuffer());
	m_driver->updateHardwareBuffer(buffer.getIndexBuffer());
}

bool Tiles::evict()
{
	if (m_bytes <= KEEP_BYTES)
		return false;
	// the least recently wanted first, down to the budget; nothing wanted
	// lately (the roots are wanted every update: kept). Not just this update's:
	// a tile drawn whole for an update doesn't want its children, and over the
	// budget they went, so the next it split again and found them gone: its
	// oldest kept ancestor stood in, a kilometre-wide tile flashing at the horizon.
	size_t gone = 0;
	std::vector<std::pair<u32, Key>> old;
	for (const auto &[key, e] : m_tiles)
		if (e.used + KEEP_UPDATES < m_frame && m_tiling.top(key.face) - key.level >= KEEP_COARSE)
			old.emplace_back(e.used, key);
	std::sort(old.begin(), old.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
	for (const auto &[used, key] : old) {
		if (m_bytes <= KEEP_BYTES)
			break;
		auto it = m_tiles.find(key);
		m_bytes -= it->second.bytes;
		m_ram -= it->second.ram;
		m_tiles.erase(it);
		gone++;
	}
	m_stats.evicted(gone);
	return gone > 0;
}

} // namespace far
