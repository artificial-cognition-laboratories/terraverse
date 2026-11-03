// A contact sheet of the figure's moves, as PNGs, so a pose can be looked at without the game.
// Runs the same pure code the game does (physics/pose, reach, skeleton); only the driving here
// is a script, standing in for client/body. Each move: side and three-quarter, frame by frame.
//   bun pose-sheet <out-dir>
#include "physics/reach.h"
#include "physics/skeleton.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace physics;

static uint32_t crc(const unsigned char *p, size_t n, uint32_t c = 0xffffffffu)
{
	for (size_t i = 0; i < n; i++) {
		c ^= p[i];
		for (int k = 0; k < 8; k++)
			c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
	}
	return c;
}
static void be32(std::vector<unsigned char> &b, uint32_t v)
{
	for (int s = 24; s >= 0; s -= 8)
		b.push_back(v >> s);
}
static void chunk(FILE *f, const char *type, const std::vector<unsigned char> &data)
{
	std::vector<unsigned char> b;
	be32(b, data.size());
	b.insert(b.end(), type, type + 4);
	b.insert(b.end(), data.begin(), data.end());
	be32(b, ~crc(b.data() + 4, b.size() - 4));
	fwrite(b.data(), 1, b.size(), f);
}
static void png(const std::string &path, int w, int h, const std::vector<unsigned char> &rgb)
{
	FILE *f = fopen(path.c_str(), "wb");
	if (!f)
		throw std::runtime_error("can't write " + path);
	static const unsigned char sig[] = { 137, 80, 78, 71, 13, 10, 26, 10 };
	fwrite(sig, 1, 8, f);
	std::vector<unsigned char> head;
	be32(head, w);
	be32(head, h);
	head.insert(head.end(), { 8, 2, 0, 0, 0 });
	chunk(f, "IHDR", head);
	std::vector<unsigned char> raw;
	for (int y = 0; y < h; y++) {
		raw.push_back(0);
		raw.insert(raw.end(), rgb.begin() + y * w * 3, rgb.begin() + (y + 1) * w * 3);
	}
	std::vector<unsigned char> z{ 0x78, 1 };
	uint32_t a = 1, b = 0;
	for (unsigned char c : raw) {
		a = (a + c) % 65521;
		b = (b + a) % 65521;
	}
	for (size_t at = 0; at < raw.size();) {
		const size_t n = std::min<size_t>(65535, raw.size() - at);
		z.push_back(at + n == raw.size());
		z.push_back(n & 255);
		z.push_back(n >> 8);
		z.push_back(~n & 255);
		z.push_back((~n >> 8) & 255);
		z.insert(z.end(), raw.begin() + at, raw.begin() + at + n);
		at += n;
	}
	be32(z, (b << 16) | a);
	chunk(f, "IDAT", z);
	chunk(f, "IEND", {});
	fclose(f);
}

struct Colour { unsigned char r, g, b; };
static const Colour SKIN{ 216, 160, 122 }, SHIRT{ 0, 148, 210 }, TROUSERS{ 58, 63, 85 }, BLOCK{ 150, 150, 145 }, BACK{ 235, 235, 230 };

struct Image
{
	int w, h;
	std::vector<unsigned char> rgb;
	std::vector<float> depth;
	Image(int w, int h) : w(w), h(h), rgb(w * h * 3), depth(w * h, 1e9f)
	{
		for (int i = 0; i < w * h; i++) {
			rgb[i * 3] = BACK.r; rgb[i * 3 + 1] = BACK.g; rgb[i * 3 + 2] = BACK.b;
		}
	}
	void put(int x, int y, float z, Colour c, float shade)
	{
		if (x < 0 || y < 0 || x >= w || y >= h || z >= depth[y * w + x])
			return;
		depth[y * w + x] = z;
		rgb[(y * w + x) * 3] = c.r * shade; rgb[(y * w + x) * 3 + 1] = c.g * shade; rgb[(y * w + x) * 3 + 2] = c.b * shade;
	}
	void clearDepth() { std::fill(depth.begin(), depth.end(), 1e9f); }
};

// A view of a cell of the sheet: how the world's axes fall on it
struct View
{
	int cx, cy, half; // middle of its foot line, half a cell's width (px)
	float yaw;        // degrees round the figure
	float scale;      // px a node
	v3f centre;
	void project(v3f p, float &x, float &y, float &z) const
	{
		const float a = yaw * 3.14159265f / 180, c = std::cos(a), s = std::sin(a);
		const v3f q = p - centre;
		x = cx + (q.X * c - q.Z * s) * scale;
		z = q.X * s + q.Z * c;
		y = cy - q.Y * scale;
	}
};

// A box under a matrix: its faces as quads, by depth
static void box(Image &im, const View &v, const core::matrix4 &m, v3f size, v3f centre, Colour col)
{
	const v3f h = size / 2;
	static const int FACE[6][4] = { { 0, 1, 3, 2 }, { 4, 5, 7, 6 }, { 0, 1, 5, 4 }, { 2, 3, 7, 6 }, { 0, 2, 6, 4 }, { 1, 3, 7, 5 } };
	static const float LIT[6] = { 0.75f, 0.75f, 0.6f, 1.f, 0.85f, 0.85f };
	v3f p[8];
	for (int i = 0; i < 8; i++) {
		p[i] = centre + v3f(i & 4 ? h.X : -h.X, i & 2 ? h.Y : -h.Y, i & 1 ? h.Z : -h.Z);
		m.transformVect(p[i]);
	}
	for (int f = 0; f < 6; f++) {
		float x[4], y[4], z[4];
		for (int i = 0; i < 4; i++)
			v.project(p[FACE[f][i]], x[i], y[i], z[i]);
		const float x0 = std::min({ x[0], x[1], x[2], x[3] }), x1 = std::max({ x[0], x[1], x[2], x[3] });
		const float y0 = std::min({ y[0], y[1], y[2], y[3] }), y1 = std::max({ y[0], y[1], y[2], y[3] });
		for (int py = std::floor(y0); py <= std::ceil(y1); py++)
			for (int px = std::max<int>(std::floor(x0), v.cx - v.half); px <= std::min<int>(std::ceil(x1), v.cx + v.half); px++) {
				// two triangles
				for (const int t : { 1, 3 }) {
					const float ax = x[0], ay = y[0], bx = x[t == 1 ? 1 : 2], by = y[t == 1 ? 1 : 2], cx = x[t == 1 ? 2 : 3], cy = y[t == 1 ? 2 : 3];
					const float d = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
					if (std::fabs(d) < 1e-3f)
						continue;
					const float u = ((px - ax) * (cy - ay) - (cx - ax) * (py - ay)) / d, w = ((bx - ax) * (py - ay) - (px - ax) * (by - ay)) / d;
					if (u < 0 || w < 0 || u + w > 1)
						continue;
					const float zz = z[0] + (z[t == 1 ? 1 : 2] - z[0]) * u + (z[t == 1 ? 2 : 3] - z[0]) * w;
					im.put(px, py, zz, col, LIT[f]);
				}
			}
	}
}

static void drawFigure(Image &im, const View &v, const Bones &b, v3f foot)
{
	const float S = FIGURE_SCALE;
	core::matrix4 root;
	root.setTranslation(foot);
	const auto hanging = [&](const core::matrix4 &m, float length, float wide, Colour c) {
		box(im, v, m, v3f(wide, length + BONE_OVERLAP, wide) * S, v3f(0, (BONE_OVERLAP - length) / 2, 0) * S, c);
	};
	const core::matrix4 lower = root * b.lower.matrix(), hips = lower * b.hips.matrix(), chest = hips * b.chest.matrix(), head = chest * b.head.matrix();
	box(im, v, hips, v3f(0.48f, HIPS_TALL, 0.23f) * S, v3f(0, HIPS_TALL / 2, 0) * S, TROUSERS);
	box(im, v, chest, v3f(0.5f, CHEST_TALL + 0.02f, BONE_WIDE) * S, v3f(0, CHEST_TALL / 2, 0) * S, SHIRT);
	box(im, v, head, v3f(HEAD_SIZE) * S, v3f(0, HEAD_SIZE / 2, 0) * S, SKIN);
	for (int s : { 0, 1 }) {
		const core::matrix4 thigh = lower * b.thigh[s].matrix(), shin = thigh * b.shin[s].matrix();
		hanging(thigh, THIGH, BONE_WIDE, TROUSERS);
		hanging(shin, SHIN, BONE_NARROW, TROUSERS);
		const core::matrix4 upper = chest * b.upper[s].matrix(), fore = upper * b.fore[s].matrix(), hand = fore * b.hand[s].matrix();
		hanging(upper, UPPER_ARM, BONE_WIDE, SHIRT);
		hanging(fore, FOREARM, BONE_NARROW, SHIRT);
		box(im, v, hand, v3f(BONE_WIDE, HAND_LONG, BONE_WIDE) * S, v3f(0, -HAND_LONG / 2, 0) * S, SKIN);
	}
}

// One moment of a move, as client/body and client/figure would give it (figure faces +z, unturned)
struct Frame
{
	v3f foot, moving;
	PoseIn in;
	std::optional<Grip> grip; // world
};
using Script = std::function<Frame(float)>;
struct Sheet
{
	std::string name;
	float time;
	std::vector<std::pair<v3f, v3f>> blocks; // min, max
	Script script;
};

static void render(const Sheet &sheet, const std::string &dir)
{
	constexpr int CELL = 170, COLS = 10, ROWS = 3;
	Image im(CELL * COLS, CELL * ROWS);
	constexpr float DT = 1.f / 60;
	Poser poser;
	Skeleton skeleton;
	const int steps = std::ceil(sheet.time / DT);
	int cell = 0;
	v3f was = sheet.script(0).foot;
	for (int i = 0; i <= steps; i++) {
		const float t = i * DT;
		Frame f = sheet.script(t);
		PoseIn in = f.in;
		in.moving = (f.foot - was) / DT;
		if (i == 0)
			in.moving = f.moving;
		was = f.foot;
		const Joints j = poser.step(DT, in);
		std::optional<Grip> g = f.grip;
		if (g)
			for (int s : { 0, 1 })
				g->wrist[s] -= f.foot;
		const Bones b = skeleton.place(DT, j, 0, g ? &*g : nullptr);
		if (i * (COLS - 1) / steps == cell && cell < COLS) {
			for (int row = 0; row < ROWS; row++) {
				im.clearDepth();
				const View v{ CELL * cell + CELL / 2, CELL * row + CELL - 20, CELL / 2 - 1, row == 0 ? 90.f : row == 1 ? 0.f : 35.f, 50, f.foot + v3f(0, 0, 0) };
				for (const auto &[lo, hi] : sheet.blocks) {
					core::matrix4 m;
					box(im, v, m, hi - lo, (lo + hi) / 2, BLOCK);
				}
				core::matrix4 m;
				box(im, v, m, v3f(40, 0.02f, 40), v3f(f.foot.X, -0.51f, f.foot.Z), v3f(0) == v3f(1) ? BLOCK : Colour{ 190, 200, 170 });
				drawFigure(im, v, b, f.foot);
			}
			cell++;
		}
	}
	png(dir + "/" + sheet.name + ".png", im.w, im.h, im.rgb);
	printf("%s/%s.png\n", dir.c_str(), sheet.name.c_str());
}

static Grip handsOn(const Ledge &l, float forward = 0, bool right = true, bool left = true)
{
	Grip g;
	const v3f along(-l.normal.Z, 0, l.normal.X), at = l.lip + v3f(0, 0.06f, 0) - l.normal * forward;
	g.wrist[0] = at + along * 0.34f;
	g.wrist[1] = at - along * 0.34f;
	g.held[0] = right;
	g.held[1] = left;
	g.over = l.normal * -1.f;
	return g;
}

// A climb along its real path, from running at it
// A catch and what follows, run by the real Haul; the speed it goes at is printed, to see any snap
static Sheet haul(const std::string &name, float height, v3f start, v3f velocity, bool pull, bool hang_first = false)
{
	Ledge l{ v3f(0, 0, -1), v3f(0, height - 0.5f, 0), false, true, -0.5f };
	Haul h = hang_first ? Haul::pullUp(l, start, 0.3f) : Haul(l, start, velocity, 0.3f, pull, -0.5f);
	std::vector<Frame> trace;
	constexpr float DT = 1.f / 60;
	float worst = 0;
	v3f was = h.velocity();
	const v3f in = l.normal * -1.f;
	for (int i = 0; i < 180 && !h.done(); i++) {
		const bool holding = h.holding(), caught = h.phase() == Haul::Phase::CATCH;
		h.step(DT);
		worst = std::max(worst, (h.velocity() - was).getLength() / DT);
		was = h.velocity();
		Frame f;
		f.foot = h.at();
		f.in.stance = caught ? Stance::HANG : Stance::MANTLE;
		f.in.faces = true;
		if (holding)
			f.grip = handsOn(l);
		if (const std::optional<v3f> on = h.stepOn()) {
			f.in.steps = true;
			f.in.step_on = (*on - h.at()) / FIGURE_SCALE;
		}
		trace.push_back(f);
	}
	printf("%s: %d frames, %.2f s, hardest change of speed %.0f nodes/s2\n", name.c_str(), (int)trace.size(), trace.size() * DT, worst);
	for (int i = 0; i < 18; i++) { // on, off the top
		Frame f;
		f.foot = trace.back().foot + h.exit() * (i * DT);
		trace.push_back(f);
	}
	Sheet s{ name, (trace.size() - 1) * DT, { { v3f(-3, -0.5f, 0), v3f(3, height - 0.5f, 3) } }, nullptr };
	s.script = [trace](float t) { return trace[std::min<size_t>(std::round(t * 60), trace.size() - 1)]; };
	return s;
}

static Sheet climb(const std::string &name, Move move, float height, bool thin, float pace, float off)
{
	Ledge l{ v3f(0, 0, -1), v3f(0, height - 0.5f, 0), thin, true, -0.5f };
	const float a = off * core::DEGTORAD;
	const v3f way(std::sin(a), 0, std::cos(a));
	const v3f start(-way.X * 1.f, -0.5f, -0.9f);
	const ClimbPath path = climbPath(move, l, start, 0.3f, pace);
	const float lead = 0.25f, after = 0.5f;
	Sheet s{ name, lead + path.time + after, { { v3f(-3, -0.5f, 0), v3f(3, height - 0.5f, thin ? 1.f : 3.f) } }, nullptr };
	s.script = [=](float t) {
		Frame f;
		f.in.pitch = 0.2f;
		f.moving = way * pace;
		if (t < lead) { // running in
			f.foot = start - way * ((lead - t) * pace);
			return f;
		}
		const float c = t - lead;
		if (c < path.time) {
			f.foot = path.at(c);
			f.in.stance = move.climb == Climb::VAULT ? Stance::VAULT : Stance::MANTLE;
			f.in.faces = true;
			f.in.face = move.side ? 1.2f : 0;
			constexpr float LETS_GO_VAULT = 0.65f, LETS_GO_MANTLE = 0.85f;
			if (c / path.time < (move.climb == Climb::VAULT ? LETS_GO_VAULT : LETS_GO_MANTLE))
				f.grip = handsOn(l, 0.15f, !move.side, true);
			if (c / path.time < 0.8f && (move.side || move.dive))
				f.grip = handsOn(l, 0.15f, !move.side, true);
			const float swing = std::sin(3.14159265f * c / path.time);
			f.in.throw_side = move.side ? 0.9f * swing : 0;
			f.in.throw_back = move.dive ? 1.1f * swing : 0;
			return f;
		}
		f.foot = path.to + way * ((c - path.time) * pace);
		return f;
	};
	return s;
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: sheet <out-dir>\n");
		return 1;
	}
	const std::string dir = argv[1];
	std::vector<Sheet> sheets;
	const auto ground = [](float pace, Stance stance, bool air = false) {
		return [=](float t) {
			Frame f;
			f.foot = v3f(0, -0.5f, pace * t);
			f.in.stance = stance;
			f.in.airborne = air;
			f.in.pitch = 0.2f;
			f.moving = v3f(0, 0, pace);
			return f;
		};
	};
	sheets.push_back({ "stand", 1.f, {}, ground(0, Stance::STAND) });
	sheets.push_back({ "walk", 0.8f, {}, ground(4, Stance::STAND) });
	sheets.push_back({ "run", 0.6f, {}, ground(6.4f, Stance::STAND) });
	sheets.push_back({ "crouch", 0.8f, {}, ground(2, Stance::CROUCH) });
	sheets.push_back({ "slide", 0.8f, {}, ground(6, Stance::SLIDE) });
	sheets.push_back({ "roll", 0.6f, {}, ground(6, Stance::ROLL) });
	sheets.push_back({ "jump", 0.9f, {}, [](float t) {
		Frame f;
		const float air = t - 0.15f;
		f.foot = v3f(0, -0.5f + (air > 0 ? std::max(0.f, 5.f * air - 4.9f * air * air) : 0), 6 * t);
		f.in.airborne = f.foot.Y > -0.5f;
		f.in.above = f.foot.Y + 0.5f;
		f.moving = v3f(0, 0, 6);
		f.moving = v3f(0, 0, 6);
		return f;
	} });
	sheets.push_back(climb("kong", { "kong", Climb::VAULT, true, false, true }, 1, true, 6, 0));
	sheets.push_back(climb("speed-vault", { "speed vault", Climb::VAULT, true, false }, 1, true, 3.5f, 15));
	sheets.push_back(climb("side-vault", { "side vault", Climb::VAULT, true, true }, 1, true, 5, 40));
	sheets.push_back(climb("onto", { "onto it", Climb::VAULT, false, false }, 1, false, 5, 0));
	sheets.push_back(haul("mantle-rising", 2, v3f(0, 0.2f, -0.5f), v3f(0, 3, 5), true));
	sheets.push_back(haul("mantle-falling", 3, v3f(0, 1.1f, -0.6f), v3f(0, -4, 4), true));
	sheets.push_back(haul("hard-catch", 3, v3f(0, 1.1f, -0.6f), v3f(0, -8, 2), false));
	const Ledge edge{ v3f(0, 0, -1), v3f(0, 2.5f, 0), false, true };
	const v3f hung(0, edge.lip.Y - HANG_DROP, -0.35f);
	const std::vector<std::pair<v3f, v3f>> wall{ { v3f(-3, -0.5f, 0), v3f(3, 2.5f, 2) } };
	sheets.push_back({ "catch-and-hang", 1.2f, wall, [=](float t) {
		Frame f;
		f.in.stance = t < 0.2f ? Stance::STAND : Stance::HANG;
		f.in.airborne = t < 0.2f;
		f.in.faces = t >= 0.2f;
		if (t < 0.2f)
			f.foot = hung + v3f(0, 0.4f - 2 * t, -0.8f + 4 * t);
		else {
			f.foot = hung;
			f.grip = handsOn(edge);
		}
		f.in.pitch = -0.2f;
		return f;
	} });
	sheets.push_back({ "shimmy", 1.2f, wall, [=](float t) {
		Frame f;
		f.in.stance = Stance::HANG;
		f.in.faces = true;
		f.foot = hung + v3f(1.5f * t, 0, 0);
		Ledge l = edge;
		l.lip.X = f.foot.X;
		// (hand over hand is client/body's: here each hand steps in turn)
		static HandOverHand hands;
		const Grip g = handsOn(l);
		if (t == 0)
			hands.place(g.wrist[0], g.wrist[1]);
		hands.step(1.f / 60, g.wrist[0], g.wrist[1]);
		f.grip = g;
		f.grip->wrist[0] = hands.at(0);
		f.grip->wrist[1] = hands.at(1);
		f.in.hand_moving = hands.moving();
		return f;
	} });
	sheets.push_back(haul("pull-up", 3, hung, v3f(0), true, true));
	sheets.push_back({ "wall-run", 0.8f, { { v3f(0.3f, -0.5f, -8), v3f(1.3f, 3, 8) } }, [](float t) {
		Frame f;
		f.in.stance = Stance::WALL_RUN;
		f.foot = v3f(0, -0.5f + 2 * t - 1.7f * t * t + 0.3f, 6 * t);
		return f;
	} });
	sheets.push_back({ "catch-running", 2.f, wall, [=](float t) {
		Frame f;
		f.in.pitch = -0.2f;
		if (t < 0.25f) { // along the wall, in the air, then caught
			f.foot = hung + v3f(5 * (t - 0.25f), 0, 0);
			f.in.airborne = true;
			return f;
		}
		f.foot = hung;
		f.in.stance = Stance::HANG;
		f.in.faces = true;
		f.grip = handsOn(edge);
		return f;
	} });
	for (const float pace : { 2.f, 6.f }) {
		// the real push: square at a wall, falling a little
		const v3f normal(0, 0, -1), face(0, 0, 0.3f);
		v3f at(0, 0.2f, -0.2f);
		WallPush push(normal, v3f(0, -1, pace), 5, face);
		std::vector<Frame> trace;
		for (int i = 0; i < 40; i++) {
			Frame f;
			f.in.airborne = true;
			f.in.pitch = 0.1f;
			const bool on = !push.done();
			const v3f v = on ? push.step(1.f / 60) : trace.back().moving + v3f(0, -9.8f / 60, 0);
			at += v / 60.f;
			f.foot = at;
			f.moving = v;
			if (on) {
				f.in.steps = f.in.faces = true;
				f.in.feet = 2;
				f.in.step_on = (push.footOn() - at) / FIGURE_SCALE;
				if (push.handsOn()) {
					Grip g;
					g.held[0] = g.held[1] = true;
					g.wrist[0] = push.handsAt() + v3f(0.3f, 0, 0);
					g.wrist[1] = push.handsAt() - v3f(0.3f, 0, 0);
					g.over = v3f(0, 1, 0);
					f.grip = g;
				}
			} else {
				f.in.push = v3f(0, 0, std::max(0.f, 1 - (i * 1.f / 60 - push.pressTime() - WallPush::SPRING) / 0.35f));
			}
			trace.push_back(f);
		}
		Sheet s{ pace > 4 ? "wall-kick-fast" : "wall-kick-slow", 39.f / 60, { { v3f(-3, -0.5f, 0.3f), v3f(3, 3, 1.3f) } }, nullptr };
		s.script = [trace](float t) { return trace[std::min<size_t>(std::round(t * 60), trace.size() - 1)]; };
		sheets.push_back(s);
	}
	sheets.push_back({ "trip", 0.9f, { { v3f(-3, -0.5f, 0.5f), v3f(3, 0, 6) } }, [](float t) {
		// shin caught the top of a half-node step at a run, at t = 0.1
		Frame f;
		const float since = t - 0.1f;
		const float pace = since < 0 ? 6.f : 2.7f;
		f.foot = v3f(0, -0.5f + (since > 0 ? std::min(0.5f, since * 1.5f) : 0), since < 0 ? 6 * since + 0.25f : 0.25f + 2.7f * since);
		f.moving = v3f(0, 0, pace);
		f.in.stagger = since > 0 ? std::max(0.f, 1 - 2 * since) : 0;
		f.in.pitch = 0.2f;
		if (since > 0 && since < 0.3f) {
			Grip g;
			g.held[0] = g.held[1] = true;
			g.wrist[0] = v3f(0.25f, 0.06f, 0.75f);
			g.wrist[1] = v3f(-0.25f, 0.06f, 0.75f);
			g.over = v3f(0, 0, 1);
			f.grip = g;
		}
		return f;
	} });
	for (const bool front : { true, false }) {
		Fallen down(0.4f, front);
		std::vector<Frame> trace;
		v3f at(0, -0.5f, 0);
		float pace = 6;
		for (int i = 0; i < 180 && !down.done(); i++) {
			down.step(1.f / 60, false);
			pace = std::max(0.f, pace - 7.f / 60);
			at.Z += (front ? pace : -pace) / 60;
			Frame f;
			f.foot = at;
			f.in.stance = Stance::DOWN;
			f.in.faces = true;
			f.in.fallen = down.angle();
			f.in.rising = down.riseAt();
			if (down.handsDown()) {
				Grip g;
				g.held[0] = g.held[1] = true;
				const v3f hands = at + v3f(0, 0.06f, front ? 0.45f : -0.45f);
				g.wrist[0] = hands + v3f(0.3f, 0, 0);
				g.wrist[1] = hands - v3f(0.3f, 0, 0);
				g.over = v3f(0, 0, front ? 1.f : -1.f);
				f.grip = g;
			}
			trace.push_back(f);
		}
		Sheet s{ front ? "fall-front" : "fall-back", (trace.size() - 1) / 60.f, { { v3f(-6, -1.5f, -8), v3f(6, -0.5f, 12) } }, nullptr };
		s.script = [trace](float t) { return trace[std::min<size_t>(std::round(t * 60), trace.size() - 1)]; };
		sheets.push_back(s);
	}
	sheets.push_back({ "turn", 1.2f, {}, [](float t) {
		Frame f;
		const float a = t * 2.5f;
		f.foot = v3f(2.4f * (1 - std::cos(a)), -0.5f, 2.4f * std::sin(a));
		return f;
	} });
	for (const Sheet &s : sheets) {
		// (in `script`, HandOverHand's state and frame order are the sheet's own, one pass each)
		render(s, dir);
	}
	return 0;
}
