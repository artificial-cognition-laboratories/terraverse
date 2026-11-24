// Terra Firma: the check's probe of an atlas's seams, crossed (sprint 119: was a Lua mod in
// tests/checks/atlas-seam.sh; layout v2: planet.paper.md, section 3). See probe.h.
//
// Courses of stone are laid along real paths on the planet, so each is whole across the
// seam it tests; then, over each, a walker crosses at the floor's height (never in seam,
// never jumping on the planet, still going, ending up past the seam) and water poured
// before the seam flows past it. And on a segment's seam, sand dropped above lands below.
#include "tests/probe/probe.h"

#include "src/shared/game/objects/collision.h"
#include "src/shared/map/map.h"
#include "src/server/app/server.h"
#include "src/server/game/kinds.h"
#include "src/server/world/run/rules/matter.h"
#include "src/server/world/state/objects/entity_sao.h"
#include "src/server/world/state/environment/serverenvironment.h"
#include <cmath>
#include <map>
#include <memory>
#include <set>

namespace probe {

namespace {

constexpr s64 FLOOR = 120;  // the courses' layer: above the terrain, below the top
constexpr float SPEED = 3;  // m/s
constexpr double HALF = 2;  // blocks each side of a course's line
const char WALKER[] = "probe:walker";

struct Tally
{
	int pass = 0, fail = 0;
	void check(Context &c, bool ok, const std::string &what)
	{
		if (ok) {
			pass++;
			return;
		}
		if (++fail <= 30)
			c.say("ATLAS miss " + what);
	}
};

std::string str(v3f p)
{
	return format("(%.2f,%.2f,%.2f)", p.X, p.Y, p.Z);
}

// the node a mover is in: a centimetre above its feet (planet::Atlas::moverNode)
v3s16 moverNode(v3f p)
{
	return v3s16(std::floor(p.X + 0.5), std::floor(p.Y + 0.51), std::floor(p.Z + 0.5));
}

// A course: s along it (blocks, the seam at 0), w across it, to latitude and longitude.
using Path = std::function<std::pair<double, double>(double s, double w)>;
struct Course
{
	std::string name;
	Path path;
	int axis;  // 0 x, 2 z: which way the walker goes in storage
	int sign;
	std::function<bool(double lat, double lon)> past;
	std::vector<v3s16> floor, air, near;
	v3s16 spring, beyond;
};

// A walker: on each step, never in seam, never jumping on the planet.
class Walker : public EntityKind
{
public:
	Walker(Context &c, std::shared_ptr<Tally> t) : m_c(c), m_t(std::move(t)) {}
	void activate(EntitySAO &self, const std::string &, u32) override
	{
		ObjectProperties &p = *self.accessObjectProperties();
		p.physical = true;
		p.collideWithObjects = false;
		p.collisionbox = aabb3f(-0.3f, 0, -0.3f, 0.3f, 1.7f, 0.3f);
		p.visual = OBJECTVISUAL_SPRITE;
		p.textures = { "blank.png" };
		p.static_save = false;
	}
	std::string state(const EntitySAO &) const override { return ""; }
	void step(EntitySAO &self, float dt, const CollisionMoveResult *) override
	{
		if (case_.empty())
			return;
		const v3f pos = self.getBasePosition() / BS;
		m_t->check(m_c, m_c.cell(moverNode(pos)).has_value(), case_ + ": walker in seam at " + str(pos));
		const v3s16 n = moverNode(pos);
		const auto here = m_c.atlas().point({ n.X, n.Y, n.Z }, pos.X, pos.Y, pos.Z);
		if (here && m_last) {
			const double moved = std::sqrt(std::pow(here->x - m_last->x, 2) + std::pow(here->y - m_last->y, 2) +
					std::pow(here->z - m_last->z, 2));
			m_t->check(m_c, moved <= SPEED * dt + 1, case_ + format(": jumped %.2f blocks in %.3f s", moved, dt));
		}
		m_last = here;
	}
	std::string case_;

private:
	Context &m_c;
	std::shared_ptr<Tally> m_t;
	std::optional<terragen::Atlas::Vec> m_last;
};

} // namespace

void atlasSeams(Context &c)
{
	auto t = std::make_shared<Tally>();
	registerProbeKind(WALKER, [&c, t] { return std::make_unique<Walker>(c, t); });
	const double R = c.atlas().body(planet::Atlas::HOME).params().radius;
	const std::string STONE = "core:stone", SAND = "core:sand", WATER = "core:water_source", PUDDLE = "core:water_puddle";
	// the server's steps: how long, and how many
	struct Steps { long n = 0; float longest = 0; };
	auto steps = std::make_shared<Steps>();
	c.eachStep([steps](float dtime) {
		steps->n++;
		steps->longest = std::max(steps->longest, dtime);
	});
	const auto node = [&c](double lat, double lon, s64 layer) { return *c.atlasNode(lat, lon, layer); };

	// Seams, found from geometry: segments' meridians, every 360°/segments from longitude 0;
	// the caps' circles.
	int segments = 0;
	for (const planet::Atlas::Tile &r : c.atlas().tiles())
		segments += r.kind == terragen::Atlas::Region::Segment;
	const double WIDTH = 2 * M_PI / segments;
	const auto capEdge = [&c, node, R](double lon) {
		for (double lat = 80 * M_PI / 180; lat < 89 * M_PI / 180; lat += 1 / (3 * R)) {
			const auto cell = c.cell(node(lat, lon, FLOOR));
			if (cell && cell->region == terragen::Atlas::Region::Cap)
				return lat;
		}
		throw BaseException("atlas seam probe: no cap");
	};
	const auto eastward = [R](double lat, double lon0) -> Path {
		return [=](double s, double w) { return std::pair{ lat + w / R, lon0 + s / (R * std::cos(lat)) }; };
	};
	const auto northward = [R](double lat0, double lon) -> Path {
		return [=](double s, double w) { return std::pair{ lat0 + s / R, lon + w / (R * std::cos(lat0 + s / R)) }; };
	};
	auto courses = std::make_shared<std::vector<Course>>();
	const double cap0 = capEdge(0), cap45 = capEdge(M_PI / 4);
	const double seam1 = WIDTH, seam5 = 5 * WIDTH;
	const double cos45 = std::cos(M_PI / 4), cos30 = std::cos(M_PI / 6);
	courses->push_back({ "a segment's seam at longitude 0, east at the equator", eastward(0.4 / R, 0), 0, 1,
		[R](double, double lon) { return lon > 0 && lon < 6 / R; } });
	courses->push_back({ "a segment's seam, east at the equator", eastward(0.4 / R, seam1), 0, 1,
		[=](double, double lon) { return lon > seam1 + 1 / R; } });
	courses->push_back({ "a segment's seam, east near 45°", eastward(M_PI / 4, seam5), 0, 1,
		[=](double, double lon) { return lon > seam5 + 1 / (R * cos45); } });
	const Path west30 = eastward(-M_PI / 6, seam5);
	courses->push_back({ "a segment's seam, west near 30° south", [west30](double s, double w) { return west30(-s, w); }, 0, -1,
		[=](double, double lon) { return lon < seam5 - 1 / (R * cos30); } });
	courses->push_back({ "into the cap at 0°", northward(cap0, 0), 2, 1, [=](double lat, double) { return lat > cap0 + 1 / R; } });
	courses->push_back({ "into the cap at 45°", northward(cap45, M_PI / 4), 2, 1, [=](double lat, double) { return lat > cap45 + 1 / R; } });

	// Every node a course needs: its floor, and air three layers over it; and what's round
	// it loaded (collision takes an unloaded node as solid, and across a seam it looks into
	// the far grid's storage, round its cells).
	const auto lay = [node](Course &k) {
		// a tenth of a block apart: thin cut cells at the seam are cells too
		for (int si = -120; si <= 120; si++)
			for (double w = -HALF; w <= HALF + 1e-9; w += 0.25) {
				const auto [lat, lon] = k.path(si / 10.0, w);
				k.floor.push_back(node(lat, lon, FLOOR));
				for (int up = 1; up <= 3; up++)
					k.air.push_back(node(lat, lon, FLOOR + up));
			}
		for (int s = -14; s <= 14; s++)
			for (double w = -HALF - 3; w <= HALF + 3 + 1e-9; w += 1) {
				const auto [lat, lon] = k.path(s, w);
				for (s64 layer = FLOOR - 2; layer <= FLOOR + 5; layer++)
					k.near.push_back(node(lat, lon, layer));
			}
	};

	const auto walk = [&c, t, courses, node](std::function<void()> done) {
		struct Going { const Course *course; EntitySAO *obj; };
		auto going = std::make_shared<std::vector<Going>>();
		for (const Course &k : *courses) {
			const auto [lat, lon] = k.path(-6, 0);
			const v3s16 start = node(lat, lon, FLOOR + 1);
			auto obj = std::make_unique<EntitySAO>(&c.env(), v3f(start.X, start.Y - 0.5f, start.Z) * BS, WALKER, "");
			EntitySAO *o = obj.get();
			if (c.env().addActiveObject(std::move(obj)) == 0)
				throw BaseException("atlas seam probe: no walker");
			o->setAcceleration(v3f(0, -9.81f, 0) * BS);
			v3f v(0, 0, 0);
			(k.axis == 0 ? v.X : v.Z) = SPEED * k.sign;
			o->setVelocity(v * BS);
			dynamic_cast<Walker &>(*o->kind()).case_ = k.name;
			going->push_back({ &k, o });
		}
		c.after(4, [&c, t, going, done] {
			for (const Going &g : *going) {
				const v3f p = g.obj->getBasePosition() / BS, v = g.obj->getVelocity() / BS;
				const auto cell = c.cell(moverNode(p));
				t->check(c, cell && cell->k == FLOOR + 1, g.course->name + ": walker at layer " +
						(cell ? std::to_string(cell->k) : std::string("nil")) + ", not on the floor");
				const auto place = c.place(moverNode(p));
				const bool past = place && g.course->past(place->first, place->second);
				t->check(c, past, g.course->name + ": walker didn't get past the seam");
				if (!past) {
					const auto [la0, lo0] = g.course->path(-6, 0);
					const auto [la1, lo1] = g.course->path(6, 0);
					c.say(format("ATLAS trace %s: from %.4f°, %.4f° toward %.4f°, %.4f°; at %s", g.course->name.c_str(),
							la0 * 180 / M_PI, lo0 * 180 / M_PI, la1 * 180 / M_PI, lo1 * 180 / M_PI, str(p).c_str()));
				}
				t->check(c, std::sqrt(v.X * v.X + v.Z * v.Z) > SPEED * 0.9, g.course->name + ": walker stopped");
				g.obj->markForRemoval();
			}
			done();
		});
	};

	const auto pour = [&c, t, courses, WATER, PUDDLE](std::function<void()> done) {
		for (Course &k : *courses) {
			const auto [la, lo] = k.path(-1.5, 0);
			k.spring = *c.atlasNode(la, lo, FLOOR + 1);
			const auto [lb, lob] = k.path(1.5, 0);
			k.beyond = *c.atlasNode(lb, lob, FLOOR + 1);
			t->check(c, c.node(k.beyond) == "air", k.name + ": past the seam isn't dry to start");
		}
		// a spring, kept pouring: one block of water spreads to a film too thin to keep
		auto again = std::make_shared<std::function<void(int)>>();
		*again = [&c, courses, WATER, self = std::weak_ptr(again)](int left) {
			for (const Course &k : *courses) {
				c.set(k.spring, WATER);
				terraMatterDisturb(c.server(), k.spring); // as a player's placing does: water moves only once woken
			}
			if (const auto next = self.lock(); next && left > 0)
				c.after(0.5f, [next, left] { (*next)(left - 1); });
		};
		(*again)(12);
		c.after(8, [&c, t, courses, done, WATER, PUDDLE] { // into a cap's grid at 45° is three flows
			for (const Course &k : *courses) {
				// water lies in a node's open space as a fill (sprint 81): the node stays air
				const std::string n = c.node(k.beyond);
				const fill::Fill *f = c.env().getMap().getFill(k.beyond);
				t->check(c, n == PUDDLE || n == WATER || (f && f->amount > 0), k.name + ": past the seam is " + n + " with no water in it");
			}
			done();
		});
	};

	// Sand on a segment's seam: dropped from six layers up, lands on stone at the bottom of
	// a pit across the seam.
	const double DROP_LAT = 12 * M_PI / 180, DROP_LON = 13 * WIDTH;
	const s64 DROP_K = FLOOR - 10;
	const auto drop = [&c, t, node, R, DROP_LAT, DROP_LON, DROP_K, STONE, SAND](std::function<void()> done) {
		const double lat = DROP_LAT, lon = DROP_LON;
		const s64 k = DROP_K;
		const v3s16 top = node(lat, lon, k + 4);
		// a headless server has no players, so no active blocks: keep the pit's blocks,
		// both segments' storage, active (collision takes an unloaded node as solid
		// ignore, and sand touching ignore is deleted); and an open pit, not a shaft:
		// across a seam the next grid's columns are offset by up to half a block
		for (int dlat = -3; dlat <= 3; dlat++)
			for (int dlon = -3; dlon <= 3; dlon++) {
				const double la = lat + dlat / R, lo = lon + dlon / (R * std::cos(lat));
				for (s64 layer = k - 3; layer <= k + 6; layer++)
					c.keep(node(la, lo, layer));
				c.set(node(la, lo, k - 3), STONE);
				for (s64 layer = k - 2; layer <= k + 6; layer++)
					c.set(node(la, lo, layer), "air");
			}
		c.set(top, SAND);
		spawnFalling(c.env(), top, c.env().getMap().getNode(top));
		c.after(4, [&c, t, node, R, lat, lon, k, SAND, done] {
			// the two segments' columns don't line up at the seam: it settles in the column
			// it hits, within one of straight below
			bool landed = false;
			for (int dlat = -1; dlat <= 1; dlat++)
				for (double dlon = -1.5; dlon <= 1.5 + 1e-9; dlon += 0.5)
					landed = landed || c.node(node(lat + dlat / R, lon + dlon / (R * std::cos(lat)), k - 2)) == SAND;
			t->check(c, landed, format("sand on the seam at layer %lld didn't land within a column of below", (long long)k));
			done();
		});
	};

	c.after(1, [&c, t, courses, lay, node, walk, pour, drop, steps, R, DROP_LAT, DROP_LON, DROP_K, STONE] {
		std::set<std::tuple<s16, s16, s16>> blocks;
		std::vector<v3s16> need;
		const auto want = [&](v3s16 p) {
			const v3s16 b = getNodeBlockPos(p);
			if (blocks.insert({ b.X, b.Y, b.Z }).second)
				need.push_back(p);
		};
		for (Course &k : *courses) {
			lay(k);
			for (const v3s16 &p : k.near)
				want(p);
		}
		for (s64 layer = DROP_K - 5; layer <= DROP_K + 8; layer++)
			for (int dlat = -3; dlat <= 3; dlat++)
				for (int dlon = -3; dlon <= 3; dlon++)
					want(node(DROP_LAT + dlat / R, DROP_LON + dlon / R, layer));
		c.say(format("ATLAS %d courses over %d map blocks", (int)courses->size(), (int)need.size()));
		auto left = std::make_shared<size_t>(need.size());
		for (const v3s16 &p : need)
			c.emerge(p, p, [&c, t, courses, left, walk, pour, drop, steps, STONE] {
				if (--*left > 0)
					return;
				for (const Course &k : *courses) {
					for (const v3s16 &q : k.air)
						c.set(q, "air");
					for (const v3s16 &q : k.floor)
						c.set(q, STONE);
					// the walkers and the water step only in blocks kept active
					for (const v3s16 &q : k.near)
						c.keep(q);
				}
				walk([&c, t, pour, drop, steps] {
					pour([&c, t, drop, steps] {
						drop([&c, t, steps] {
							c.say(format("ATLAS steps %ld, longest %.2f s", steps->n, steps->longest));
							c.say(format("ATLAS pass %d fail %d", t->pass, t->fail));
							c.finish();
						});
					});
				});
			});
	});
}

} // namespace probe
