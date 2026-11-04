// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "scene.h"
#include "exceptions.h"
#include "filesys.h"
#include "porting.h"
#include "settings.h"
#include "planetarium/camera.h"
#include <IVideoDriver.h>
#include <algorithm>
#include <cmath>

namespace shell {

static constexpr float FOV = 0.7f;

// The glow of something this hot (K): none below about 750 K.
static video::SColor glowing(video::SColor c, double t)
{
	const double strength = std::clamp((t - 750) / 1250, 0.0, 1.0);
	if (strength <= 0)
		return c;
	const double g = std::clamp((t - 900) / 1200, 0.0, 1.0), b = std::clamp((t - 1600) / 1500, 0.0, 1.0);
	const auto mix = [&](u32 from, double to) { return (u32)std::min(255.0, from * (1 - strength * 0.7) + to * 255 * strength); };
	return video::SColor(c.getAlpha(), mix(c.getRed(), 1), mix(c.getGreen(), 0.25 + 0.75 * g), mix(c.getBlue(), 0.05 + 0.9 * b));
}

// A temperature for the heat overlay: blue cold, white at room temperature, red hot.
static video::SColor heated(double t, u32 alpha)
{
	const double x = std::clamp((t - 293) / (t > 293 ? 900 : 120), -1.0, 1.0);
	const double r = x >= 0 ? 1 : 1 + 0.7 * x, g = x >= 0 ? 1 - 0.8 * x : 1 + 0.4 * x, b = x >= 0 ? 1 - 0.95 * x : 1;
	return video::SColor(alpha, (u32)(r * 255), (u32)(g * 255), (u32)(b * 255));
}

Scene::~Scene() = default;

video::ITexture *Scene::texture(const std::string &name)
{
	const auto found = m_textures.find(name);
	if (found != m_textures.end())
		return found->second;
	// the game's own files, as staged: no world's needed to read them
	const std::string mods = porting::path_share + DIR_DELIM "games" DIR_DELIM + g_settings->get("game_id") + DIR_DELIM "mods" DIR_DELIM;
	for (const char *mod : { "tf_world", "tf_matter" }) {
		const std::string path = mods + mod + DIR_DELIM "textures" DIR_DELIM + name + ".png";
		if (!fs::PathExists(path))
			continue;
		video::ITexture *t = m_driver->getTexture(path.c_str());
		if (!t)
			throw BaseException("scene: couldn't read " + path);
		return m_textures[name] = t;
	}
	throw BaseException("scene: no texture " + name);
}

Scene::Batch &Scene::batch(video::ITexture *texture, bool clear)
{
	for (Batch &b : m_batches)
		if (b.texture == texture && b.clear == clear && b.vertices.size() < 60000)
			return b;
	float frames = 1;
	if (texture) {
		const core::dimension2du size = texture->getOriginalSize();
		frames = (float)size.Width / std::max(1u, size.Height);
	}
	m_batches.push_back({ texture, clear, frames, {}, {} });
	return m_batches.back();
}

void Scene::hide()
{
	m_shown = false;
	m_frame.reset();
	m_dragging = false;
	m_sizedX = 0;
	m_fine_on = false;
	m_fine_time = -1;
	m_fine_pace = 1;
	m_fine = FineWater();
	m_drops.clear();
	m_figure.reset();
	m_ground.reset();
	m_ground_batches.clear();
	m_ground_dirty = false;
	m_machine.reset();
	m_looks.clear();
	m_parts.clear();
	m_built_parts = false;
	m_target = v3f(0);
}

void Scene::frame(Frame frame)
{
	// a stage's liquid, in sixteenths, is run here between its steps
	m_fine_on = frame.time >= 0 && m_blocks;
	if (m_fine_on) {
		m_fine.sync(frame.x, frame.y, frame.z, frame.cells, *m_blocks);
		if (m_fine_time >= 0 && frame.time > m_fine_time) {
			const u64 now = porting::getTimeMs();
			if (now > m_fine_at)
				m_fine_pace += ((float)((frame.time - m_fine_time) * 1000 / (now - m_fine_at)) - m_fine_pace) * 0.3f;
			m_fine_at = now;
		}
		m_fine_time = frame.time;
	}
	if (frame.x != m_sizedX || frame.y != m_sizedY || frame.z != m_sizedZ) {
		m_sizedX = frame.x;
		m_sizedY = frame.y;
		m_sizedZ = frame.z;
		m_distance = std::max({ frame.x, frame.y, frame.z }) * 2.9f + 1.5f;
	}
	m_frame = std::move(frame);
	m_dirty = true;
}

// One light, top-left, as the page's bevels.
static const struct Face { v3f normal, u, v; float light; } FACES[] = {
	{ { 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, 1.0f }, { { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, -1 }, 0.45f },
	{ { 1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 }, 0.72f }, { { -1, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 }, 0.86f },
	{ { 0, 0, 1 }, { -1, 0, 0 }, { 0, 1, 0 }, 0.6f }, { { 0, 0, -1 }, { 1, 0, 0 }, { 0, 1, 0 }, 0.8f },
};

static video::SColor lit(video::SColor c, float light)
{
	return video::SColor(c.getAlpha(), (u32)(c.getRed() * light), (u32)(c.getGreen() * light), (u32)(c.getBlue() * light));
}

// A quad, its texture as the block's tile: a sixteenth of a block shows a sixteenth of it.
static void quad(std::vector<video::S3DVertex> &to, std::vector<u16> &ix, const v3f &m, const v3f &u, const v3f &v,
		const v3f &normal, video::SColor c, v2f uv0, v2f uvSize, float frames)
{
	const u16 first = (u16)to.size();
	to.emplace_back(m - u - v, normal, c, v2f(uv0.X, uv0.Y * frames));
	to.emplace_back(m + u - v, normal, c, v2f(uv0.X + uvSize.X, uv0.Y * frames));
	to.emplace_back(m + u + v, normal, c, v2f(uv0.X + uvSize.X, (uv0.Y + uvSize.Y) * frames));
	to.emplace_back(m - u + v, normal, c, v2f(uv0.X, (uv0.Y + uvSize.Y) * frames));
	for (u16 k : { 0, 2, 1, 0, 3, 2 })
		ix.push_back(first + k);
}

// A game's tile string: its file, and a tint if it's multiplied ("a.png^[multiply:#rrggbb").
static std::pair<std::string, video::SColor> tileOf(const std::string &tile)
{
	std::string name = tile.substr(0, tile.find('^'));
	if (name.size() > 4 && name.compare(name.size() - 4, 4, ".png") == 0)
		name.resize(name.size() - 4);
	video::SColor tint(255, 255, 255, 255);
	const size_t at = tile.find("^[multiply:#");
	if (at != std::string::npos && tile.size() >= at + 18) {
		const u32 v = (u32)std::stoul(tile.substr(at + 12, 6), nullptr, 16);
		tint = video::SColor(255, (v >> 16) & 255, (v >> 8) & 255, v & 255);
	}
	// (a colorize as a tint: a block's own texture, leaning that way)
	const size_t coloured = tile.find("^[colorize:#");
	if (coloured != std::string::npos && tile.size() >= coloured + 18) {
		const u32 v = (u32)std::stoul(tile.substr(coloured + 12, 6), nullptr, 16);
		const size_t colon = coloured + 18;
		const float share = colon < tile.size() && tile[colon] == ':' ? std::stoi(tile.substr(colon + 1)) / 255.f : 1.f;
		const auto lean = [&](u32 c) { return (u32)(255 + (c - 255.f) * share); };
		tint = video::SColor(255, lean((v >> 16) & 255), lean((v >> 8) & 255), lean(v & 255));
	}
	return { name, tint };
}

static video::SColor multiply(video::SColor a, video::SColor b)
{
	return video::SColor(a.getAlpha(), a.getRed() * b.getRed() / 255, a.getGreen() * b.getGreen() / 255, a.getBlue() * b.getBlue() / 255);
}

void Scene::addBox(const std::array<std::string, 3> &tiles, bool clear, v3f m, v3f s, video::SColor light)
{
	const v3f h = s * 0.5f;
	for (const Face &f : FACES) {
		const auto [name, tint] = tileOf(tiles[f.normal.Y > 0 ? 0 : f.normal.Y < 0 ? 1 : 2]);
		Batch &to = batch(texture(name), clear);
		const v3f middle = m + v3f(f.normal.X * h.X, f.normal.Y * h.Y, f.normal.Z * h.Z);
		const v3f u(f.u.X * h.X, f.u.Y * h.Y, f.u.Z * h.Z), v(f.v.X * h.X, f.v.Y * h.Y, f.v.Z * h.Z);
		const float tu = std::abs(f.u.X) * s.X + std::abs(f.u.Y) * s.Y + std::abs(f.u.Z) * s.Z;
		const float tv = std::abs(f.v.X) * s.X + std::abs(f.v.Y) * s.Y + std::abs(f.v.Z) * s.Z;
		quad(to.vertices, to.indices, middle, u, v, f.normal, lit(multiply(light, tint), f.light), v2f(0, 0), v2f(tu, tv), to.frames);
	}
}

void Scene::addPlain(video::ITexture *tex, bool clear, v3f m, v3f s, video::SColor c)
{
	const v3f h = s * 0.5f;
	Batch &to = batch(tex, clear);
	for (const Face &f : FACES) {
		const v3f middle = m + v3f(f.normal.X * h.X, f.normal.Y * h.Y, f.normal.Z * h.Z);
		const v3f u(f.u.X * h.X, f.u.Y * h.Y, f.u.Z * h.Z), v(f.v.X * h.X, f.v.Y * h.Y, f.v.Z * h.Z);
		const float tu = std::abs(f.u.X) * s.X + std::abs(f.u.Y) * s.Y + std::abs(f.u.Z) * s.Z;
		const float tv = std::abs(f.v.X) * s.X + std::abs(f.v.Y) * s.Y + std::abs(f.v.Z) * s.Z;
		quad(to.vertices, to.indices, middle, u, v, f.normal, lit(c, f.light), v2f(0, 0), v2f(tu, tv), to.frames);
	}
}

void Scene::rebuild()
{
	m_batches.clear();
	m_flames.clear();
	if (!m_frame || !m_blocks)
		return;
	const Frame &f = *m_frame;
	const auto sixteenth = [](double v) { return std::clamp((int)std::lround(v * 16), 0, 16) / 16.f; };
	size_t i = 0;
	for (int y = 0; y < f.y; y++)
		for (int z = 0; z < f.z; z++)
			for (int x = 0; x < f.x; x++, i++) {
				const sandbox::Stage::Cell &c = f.cells[i];
				const v3f corner(x - f.x / 2.0f, (float)y, z - f.z / 2.0f);
				const video::SColor light = m_heat ? heated(c.temperature, 255) : glowing(video::SColor(255, 255, 255, 255), c.temperature);
				const bool known = c.block >= 0 && (size_t)c.block < m_blocks->size();
				const sandbox::StageBlock *block = known ? &(*m_blocks)[c.block] : nullptr;
				const bool air = block && block->spec.matter == "air";
				if (block && !air) {
					const bool liquid = block->spec.state == matter::State::Liquid;
					if (liquid && m_fine_on && !m_heat)
						continue; // drawn in sixteenths (drawFine)
					video::SColor lc = light;
					if (liquid)
						lc.setAlpha(205);
					// levelled: as full as it is, to the sixty-fourth it's kept in, drawn to the sixteenth
					// (or cut, as a course's kerb: level below whole on what isn't levelled)
					const float tall = block->spec.leveled || c.level < 1 ? std::max(1 / 16.f, sixteenth(c.level)) : 1.f;
					addBox(block->tiles, liquid, corner + v3f(0.5f, tall / 2, 0.5f), v3f(1, tall, 1), lc);
				}
				// a node box's own parts (a crucible's walls)
				if (block)
					for (const auto &b : block->boxes) {
						const v3f lo(b[0], b[1], b[2]), hi(b[3], b[4], b[5]);
						addBox(block->tiles, false, corner + (lo + hi) * 0.5f, hi - lo, light);
					}
				// the liquid lying in its open space, as the world keeps it
				if (c.fill.substance >= 0 && c.fill.share > 0.005 && !m_heat && (!m_fine_on || (block && block->spec.vessel))) {
					const float deep = std::max(1 / 16.f, sixteenth(c.fill.share));
					const bool water = (size_t)c.fill.substance < m_water.size() && m_water[c.fill.substance];
					const u32 v = c.fill_colour;
					video::SColor colour = water ? video::SColor(215, 255, 255, 255) : video::SColor(215, (v >> 16) & 255, (v >> 8) & 255, v & 255);
					addPlain(texture(water ? "tf_goodvibes_water_still" : "tf_matter_liquid"), true, corner + v3f(0.5f, deep / 2, 0.5f), v3f(1, deep, 1), glowing(colour, c.temperature));
				}
				// what's loose in a vessel or in the air: a pile on its floor
				else if (air && c.main >= 0 && c.filled > 0.005 && (size_t)c.main < m_substances.size()) {
					const float inner = block->spec.vessel ? 0.64f : 1.f, floor = block->spec.vessel ? 0.1f : 0.f;
					const float deep = std::max(1 / 16.f, sixteenth(std::min(1.0, c.filled)));
					const video::SColor colour = m_heat ? light : glowing(m_substances[c.main], c.temperature);
					addPlain(texture(c.state == matter::State::Liquid ? "tf_matter_liquid" : "tf_matter_substance"), c.state == matter::State::Liquid,
							corner + v3f(0.5f, floor + deep * 0.5f, 0.5f), v3f(inner, deep, inner), colour);
				}
				if (c.vapour >= 0 && c.haze > 0.02 && (size_t)c.vapour < m_substances.size() && !m_heat) {
					video::SColor colour = m_substances[c.vapour];
					colour.setAlpha((u32)(255 * std::min(0.35, c.haze * 0.4)));
					addPlain(nullptr, true, corner + v3f(0.5f), v3f(0.98f), colour);
				}
				if (c.activity > 0.5 && c.temperature > 600)
					m_flames.push_back({ corner + v3f(0.5f), std::min(1.0f, (float)(c.activity / 2000)) });
			}
}

v2f Scene::shift() const
{
	// the turning point: the floor's middle, low in what's free
	return v2f(-m_right / std::max(1, m_rect.getWidth()), -0.5f);
}

core::matrix4 Scene::view() const
{
	core::matrix4 m;
	m.buildCameraLookAtMatrixLH(eye(), m_target, v3f(0, 1, 0));
	return m;
}

v3f Scene::eye() const
{
	return m_target + v3f(std::cos(m_pitch) * std::sin(m_yaw), std::sin(m_pitch), std::cos(m_pitch) * std::cos(m_yaw)) * m_distance;
}

void Scene::figure(std::optional<Figure> figure)
{
	m_figure = std::move(figure);
}

void Scene::ground(std::optional<Ground> ground)
{
	if (ground && m_ground && ground->version == m_ground->version && ground->width == m_ground->width)
		return;
	m_ground = std::move(ground);
	m_ground_dirty = true;
}

// Runs of a row at one height as one top, and a side only where neighbours differ: flat ground is cheap.
void Scene::buildGround()
{
	m_ground_dirty = false;
	m_ground_batches.clear();
	if (!m_ground)
		return;
	const Ground &g = *m_ground;
	const float C = g.cell, RISE = g.rise;
	const bool flat = !g.colours.empty();
	const auto [topName, topTint] = tileOf(g.tiles[0]);
	const auto [sideName, sideTint] = tileOf(g.tiles[2]);
	std::vector<Batch> kept;
	kept.swap(m_batches);
	video::ITexture *topTex = flat ? nullptr : texture(topName), *sideTex = flat ? nullptr : texture(sideName);
	const auto height = [&](int x, int z) { return g.top[(size_t)z * g.width + x]; };
	const auto colourAt = [&](int x, int z) {
		const u32 v = g.colours[(size_t)z * g.width + x];
		return video::SColor(255, (v >> 16) & 255, (v >> 8) & 255, v & 255);
	};
	for (int z = 0; z < g.depth; z++)
		for (int x = 0; x < g.width;) {
			const int h = height(x, z);
			int end = x + 1;
			while (end < g.width && height(end, z) == h && (!flat || g.colours[(size_t)z * g.width + end] == g.colours[(size_t)z * g.width + x]))
				end++;
			const float y = g.corner.Y + h * RISE, x0 = g.corner.X + x * C, x1 = g.corner.X + end * C, z0 = g.corner.Z + z * C;
			const v3f middle((x0 + x1) / 2, y, z0 + C / 2);
			const video::SColor topColour = flat ? colourAt(x, z) : lit(topTint, 1);
			Batch &top = batch(topTex, false);
			quad(top.vertices, top.indices, middle, v3f((x1 - x0) / 2, 0, 0), v3f(0, 0, C / 2), v3f(0, 1, 0), topColour,
					v2f(x * C, z * C), v2f((end - x) * C, C), top.frames);
			// its sides, down to what's beside it (or the floor at the edge)
			const auto side = [&](const Face &f, v3f at, float along, int other, float uAt, video::SColor base) {
				if (other >= h)
					return;
				const float tall = (h - other) * RISE;
				Batch &b = batch(sideTex, false);
				quad(b.vertices, b.indices, at + v3f(0, -tall / 2, 0), f.u * along, f.v * (tall / 2), f.normal, lit(base, f.light),
						v2f(uAt, other * RISE), v2f(along * 2, tall), b.frames);
			};
			for (int k = x; k < end; k++) {
				const float kx = g.corner.X + (k + 0.5f) * C;
				const video::SColor base = flat ? colourAt(k, z) : sideTint;
				side(FACES[4], v3f(kx, y, z0 + C), C / 2, z + 1 < g.depth ? height(k, z + 1) : g.floor, k * C, base);
				side(FACES[5], v3f(kx, y, z0), C / 2, z > 0 ? height(k, z - 1) : g.floor, k * C, base);
			}
			const video::SColor base = flat ? colourAt(x, z) : sideTint;
			side(FACES[2], v3f(x1, y, z0 + C / 2), C / 2, end < g.width ? height(end, z) : g.floor, z * C, base);
			side(FACES[3], v3f(x0, y, z0 + C / 2), C / 2, x > 0 ? height(x - 1, z) : g.floor, z * C, base);
			x = end;
		}
	m_ground_batches.swap(m_batches);
	m_batches.swap(kept);
}

// As the game's near water: a sixteenth-wide top for each column, on its sixteenth, and sides where it
// stands over what's beside it, in the liquid's colour catching the sky. Rows of one height are one top.
void Scene::drawFine(video::IVideoDriver *driver, float dt)
{
	if (!m_fine_on || m_heat)
		return;
	m_fine.step(dt, m_fine_pace);
	constexpr float C = 1.f / 16, SHEEN = 0.22f;
	const float ox = -m_sizedX / 2.0f, oz = -m_sizedZ / 2.0f;
	const auto tinted = [&](u32 v, float by, bool top, u32 alpha) {
		const float k = top ? SHEEN : 0;
		const auto mix = [&](u32 own, u32 sky) { return (u32)std::clamp((own * (1 - k) + sky * k) * by, 0.f, 255.f); };
		return video::SColor(alpha, mix((v >> 16) & 255, 190), mix((v >> 8) & 255, 215), mix(v & 255, 235));
	};
	const int w = m_fine.width(), d = m_fine.depth();
	std::vector<video::S3DVertex> &vs = m_fine_vertices;
	std::vector<u16> &is = m_fine_indices;
	const auto flush = [&] {
		if (vs.empty())
			return;
		video::SMaterial mat;
		mat.MaterialType = video::EMT_TRANSPARENT_VERTEX_ALPHA;
		mat.FogEnable = false;
		mat.ZWriteEnable = video::EZW_OFF;
		mat.ColorMask = video::ECP_RGB;
		driver->setMaterial(mat);
		driver->drawVertexPrimitiveList(vs.data(), (u32)vs.size(), is.data(), (u32)is.size() / 3, video::EVT_STANDARD, scene::EPT_TRIANGLES, video::EIT_16BIT);
		vs.clear();
		is.clear();
	};
	const auto put = [&](v3f m, v3f u, v3f v, v3f n, video::SColor c) {
		if (vs.size() + 4 > 60000)
			flush();
		quad(vs, is, m, u, v, n, c, v2f(), v2f(), 1);
	};
	const auto beside = [&](int x, int z) {
		if (x < 0 || z < 0 || x >= w || z >= d)
			return 0;
		const int n = z * w + x;
		return m_fine.shown(n) != FineWater::NONE ? m_fine.shown(n) : m_fine.floorAt(n);
	};
	for (int z = 0; z < d; z++)
		for (int x = 0; x < w;) {
			const int c = z * w + x;
			const s32 h = m_fine.shown(c);
			if (h == FineWater::NONE) {
				x++;
				continue;
			}
			const u32 colour = m_fine.colourAt(c);
			int end = x + 1;
			while (end < w && m_fine.shown(z * w + end) == h && m_fine.colourAt(z * w + end) == colour)
				end++;
			const float y = h * C, x0 = ox + x * C, x1 = ox + end * C, z0 = oz + z * C;
			put(v3f((x0 + x1) / 2, y, z0 + C / 2), v3f((x1 - x0) / 2, 0, 0), v3f(0, 0, C / 2), v3f(0, 1, 0), tinted(colour, 0.95f, true, 235));
			const auto side = [&](const Face &f, v3f at, float half, s32 other) {
				if (other >= h)
					return;
				const float tall = (h - other) * C;
				put(at + v3f(0, -tall / 2, 0), f.u * half, f.v * (tall / 2), f.normal, tinted(colour, f.light, false, 175));
			};
			for (int k = x; k < end; k++) {
				const float kx = ox + (k + 0.5f) * C;
				side(FACES[4], v3f(kx, y, z0 + C), C / 2, beside(k, z + 1));
				side(FACES[5], v3f(kx, y, z0), C / 2, beside(k, z - 1));
			}
			side(FACES[2], v3f(x1, y, z0 + C / 2), C / 2, beside(end, z));
			side(FACES[3], v3f(x0, y, z0 + C / 2), C / 2, beside(x - 1, z));
			x = end;
		}
	// what's being poured: sixteenth cubes falling, each throwing a little out where it lands
	if (m_frame) {
		for (const auto &[at, substance] : m_frame->pouring)
			for (int k = 0; k < 3 && m_drops.size() < 400; k++) {
				const float a = (rand() % 1000) / 1000.f * 6.283f, r = (rand() % 1000) / 1000.f * 0.12f;
				const video::SColor colour = substance < m_substances.size() ? m_substances[substance] : video::SColor(255, 63, 118, 228);
				m_drops.push_back({ v3f(ox + at.x + 0.5f + std::cos(a) * r, at.y + 0.5f, oz + at.z + 0.5f + std::sin(a) * r), v3f(0, -1, 0), colour });
			}
	}
	for (size_t i = 0; i < m_drops.size();) {
		Drop &drop = m_drops[i];
		drop.speed.Y -= 9.81f * dt;
		drop.at += drop.speed * dt;
		const int cx = (int)((drop.at.X - ox) * 16), cz = (int)((drop.at.Z - oz) * 16);
		const bool over = cx >= 0 && cz >= 0 && cx < w && cz < d;
		const float landed = over ? beside(cx, cz) * C : 0;
		if (drop.at.Y <= landed) {
			m_fine.splash(drop.at.X - ox, drop.at.Z - oz, -drop.speed.Y);
			m_drops[i] = m_drops.back();
			m_drops.pop_back();
			continue;
		}
		const float h = C / 2;
		for (const Face &f : FACES)
			put(drop.at + f.normal * h, f.u * h, f.v * h, f.normal, lit(drop.colour, f.light));
		i++;
	}
	flush();
}

void Scene::follow(v3f at)
{
	// the camera eases after it, so a stumble isn't the whole view's
	m_target += (v3f(at.X, 0, at.Z) - m_target) * 0.12f;
}

static core::matrix4 turnOf(const physics::Quat &q)
{
	core::matrix4 m;
	m[0] = 1 - 2 * (q.y * q.y + q.z * q.z); m[1] = 2 * (q.x * q.y + q.w * q.z); m[2] = 2 * (q.x * q.z - q.w * q.y);
	m[4] = 2 * (q.x * q.y - q.w * q.z); m[5] = 1 - 2 * (q.x * q.x + q.z * q.z); m[6] = 2 * (q.y * q.z + q.w * q.x);
	m[8] = 2 * (q.x * q.z + q.w * q.y); m[9] = 2 * (q.y * q.z - q.w * q.x); m[10] = 1 - 2 * (q.x * q.x + q.y * q.y);
	return m;
}

void Scene::drawBatches(video::IVideoDriver *driver, const std::vector<Batch> &batches, bool clear)
{
	for (const Batch &b : batches) {
		if (b.clear != clear)
			continue;
		video::SMaterial mat;
		mat.MaterialType = clear ? video::EMT_TRANSPARENT_VERTEX_ALPHA : video::EMT_SOLID;
		mat.FogEnable = false;
		mat.ColorMask = video::ECP_RGB;
		if (clear)
			mat.ZWriteEnable = video::EZW_OFF;
		if (b.texture) {
			mat.setTexture(0, b.texture);
			mat.TextureLayers[0].MinFilter = video::ETMINF_NEAREST_MIPMAP_NEAREST;
			mat.TextureLayers[0].MagFilter = video::ETMAGF_NEAREST;
		}
		driver->setMaterial(mat);
		driver->drawVertexPrimitiveList(b.vertices.data(), (u32)b.vertices.size(), b.indices.data(), (u32)b.indices.size() / 3,
				video::EVT_STANDARD, scene::EPT_TRIANGLES, video::EIT_16BIT);
	}
}

// Its bodies' meshes are made once, then only moved.
void Scene::drawMachine(video::IVideoDriver *driver)
{
	if (!m_machine)
		return;
	if (!m_built_parts) {
		std::vector<Batch> kept;
		kept.swap(m_batches);
		for (const std::vector<LookBox> &body : m_looks) {
			for (const LookBox &b : body)
				addBox(b.tiles, false, b.box.getCenter(), b.box.getExtent(), video::SColor(255, 255, 255, 255));
			m_parts.emplace_back();
			m_parts.back().swap(m_batches);
		}
		m_batches.swap(kept);
		m_built_parts = true;
	}
	if (m_parts.size() != m_machine->bodies.size() + 1)
		throw BaseException("scene: a machine's looks and its bodies differ");
	core::matrix4 root = turnOf(m_machine->root.turn);
	root.setTranslation(m_machine->root.at);
	for (size_t i = 0; i < m_parts.size(); i++) {
		core::matrix4 world = root;
		if (i > 0) {
			core::matrix4 own = turnOf(m_machine->bodies[i - 1].rotation);
			own.setTranslation(m_machine->bodies[i - 1].offset);
			world = root * own;
		}
		driver->setTransform(video::ETS_WORLD, world);
		drawBatches(driver, m_parts[i], false);
	}
	driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);
}

// Its boxes from the game's own table, as they stand this frame; the game colours them the same.
void Scene::drawFigure(video::IVideoDriver *driver)
{
	if (!m_figure)
		return;
	std::vector<video::S3DVertex> vertices;
	std::vector<u16> indices;
	core::matrix4 root;
	const float c = std::cos(m_figure->yaw), s = std::sin(m_figure->yaw);
	root[0] = c; root[2] = s; root[8] = -s; root[10] = c;
	root.setTranslation(m_figure->at);
	physics::eachBoneBox(m_figure->bones, [&](const core::matrix4 &bone, v3f size, v3f centre, physics::BoneColour colour) {
		const video::SColor base = colour == physics::BoneColour::SKIN ? video::SColor(255, 216, 160, 122) :
				colour == physics::BoneColour::SHIRT ? video::SColor(255, 0, 148, 210) : video::SColor(255, 58, 63, 85);
		const core::matrix4 m = root * bone;
		const v3f h = size * 0.5f;
		for (const Face &f : FACES) {
			const size_t first = vertices.size();
			quad(vertices, indices, centre + v3f(f.normal.X * h.X, f.normal.Y * h.Y, f.normal.Z * h.Z),
					v3f(f.u.X * h.X, f.u.Y * h.Y, f.u.Z * h.Z), v3f(f.v.X * h.X, f.v.Y * h.Y, f.v.Z * h.Z), f.normal, lit(base, f.light), v2f(), v2f(), 1);
			for (size_t i = first; i < vertices.size(); i++)
				m.transformVect(vertices[i].Pos);
		}
	});
	video::SMaterial mat;
	mat.MaterialType = video::EMT_SOLID;
	mat.FogEnable = false;
	mat.ColorMask = video::ECP_RGB;
	driver->setMaterial(mat);
	driver->drawVertexPrimitiveList(vertices.data(), (u32)vertices.size(), indices.data(), (u32)indices.size() / 3,
			video::EVT_STANDARD, scene::EPT_TRIANGLES, video::EIT_16BIT);
}

void Scene::draw(video::IVideoDriver *driver)
{
	if (!m_shown || m_rect.getArea() <= 0)
		return;
	m_driver = driver;
	if (m_dirty) {
		rebuild();
		m_dirty = false;
	}
	const core::dimension2du window = driver->getScreenSize();
	// its own space: what's behind the panel doesn't show through
	driver->draw2DRectangle(video::SColor(255, 13, 15, 18), m_rect);
	driver->setViewPort(m_rect);
	driver->clearBuffers(video::ECBF_DEPTH);
	const float aspect = (float)m_rect.getWidth() / std::max(1, m_rect.getHeight());
	const core::matrix4 look = view();
	driver->setTransform(video::ETS_PROJECTION, planetarium::OrbitCamera::perspective(aspect, 0.1f, 200.f, shift(), FOV));
	driver->setTransform(video::ETS_VIEW, look);
	driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);

	if (m_ground_dirty)
		buildGround();
	drawBatches(driver, m_batches, false);
	drawBatches(driver, m_ground_batches, false);
	drawFigure(driver);
	drawMachine(driver);
	// the floor it stands on: the blocks' outline, so an empty scene still has a place
	{
		const float hx = m_sizedX / 2.0f, hz = m_sizedZ / 2.0f;
		const video::SColor c(255, 58, 66, 74);
		video::S3DVertex v[4] = { { { -hx, 0, -hz }, v3f(), c, v2f() }, { { hx, 0, -hz }, v3f(), c, v2f() },
			{ { hx, 0, hz }, v3f(), c, v2f() }, { { -hx, 0, hz }, v3f(), c, v2f() } };
		const u16 ix[8] = { 0, 1, 1, 2, 2, 3, 3, 0 };
		video::SMaterial mat;
		mat.MaterialType = video::EMT_SOLID;
		mat.FogEnable = false;
		mat.ColorMask = video::ECP_RGB;
		driver->setMaterial(mat);
		driver->drawVertexPrimitiveList(v, 4, ix, 4, video::EVT_STANDARD, scene::EPT_LINES, video::EIT_16BIT);
	}
	drawBatches(driver, m_batches, true);
	{
		const u64 now = porting::getTimeMs();
		static u64 last = now;
		drawFine(driver, std::clamp((now - last) / 1000.f, 0.001f, 0.05f));
		last = now;
	}
	if (!m_flames.empty()) {
		// square sparks, stepped: each hops every few frames, not every one, as the page's motion does
		m_ticks++;
		const u32 hop = m_ticks / 4;
		const v3f right(look[0], look[4], look[8]), up(look[1], look[5], look[9]);
		std::vector<video::S3DVertex> v;
		std::vector<u16> ix;
		u32 seed = 1;
		const auto next = [&seed]() { seed = seed * 1664525u + 1013904223u; return (seed >> 8) / 16777216.0f; };
		for (const Flame &f : m_flames) {
			const int sparks = 4 + (int)(f.strength * 20);
			for (int k = 0; k < sparks && v.size() + 4 < 60000; k++) {
				seed = seed + hop * 2654435761u + k * 40503u;
				const v3f at = f.at + v3f(next() - 0.5f, next() * 0.9f - 0.45f, next() - 0.5f) * 0.85f;
				const float rise = next();
				const float s = 0.035f + 0.05f * (1 - rise);
				const video::SColor c(255, 255, (u32)(120 + 110 * (1 - rise)), (u32)(30 + 40 * (1 - rise)));
				const u16 first = (u16)v.size();
				v.emplace_back(at - right * s - up * s, v3f(), c, v2f());
				v.emplace_back(at + right * s - up * s, v3f(), c, v2f());
				v.emplace_back(at + right * s + up * s, v3f(), c, v2f());
				v.emplace_back(at - right * s + up * s, v3f(), c, v2f());
				for (u16 n : { 0, 1, 2, 0, 2, 3 })
					ix.push_back(first + n);
			}
		}
		video::SMaterial mat;
		mat.MaterialType = video::EMT_TRANSPARENT_VERTEX_ALPHA;
		mat.FogEnable = false;
		mat.BackfaceCulling = false;
		mat.ZWriteEnable = video::EZW_OFF;
		mat.ColorMask = video::ECP_RGB;
		driver->setMaterial(mat);
		driver->drawVertexPrimitiveList(v.data(), (u32)v.size(), ix.data(), (u32)ix.size() / 3,
				video::EVT_STANDARD, scene::EPT_TRIANGLES, video::EIT_16BIT);
	}
	driver->setViewPort(core::recti(0, 0, window.Width, window.Height));
}

std::optional<int> Scene::pick() const
{
	if (!m_frame || !m_over)
		return std::nullopt;
	const Frame &f = *m_frame;
	// the mouse's ray, stepped through the blocks: a few dozen at most
	const float t = std::tan(FOV / 2), aspect = (float)m_rect.getWidth() / std::max(1, m_rect.getHeight());
	const float nx = 2.f * (m_mouse.X - m_rect.UpperLeftCorner.X) / m_rect.getWidth() - 1 - shift().X;
	const float ny = 1 - 2.f * (m_mouse.Y - m_rect.UpperLeftCorner.Y) / m_rect.getHeight() - shift().Y;
	const core::matrix4 look = view();
	const v3f right(look[0], look[4], look[8]), up(look[1], look[5], look[9]), ahead(look[2], look[6], look[10]);
	v3f way = ahead + right * (nx * t * aspect) + up * (ny * t);
	way.normalize();
	const v3f from = eye();
	for (float d = 0; d < m_distance * 2 + 8; d += 0.05f) {
		const v3f p = from + way * d;
		const int x = (int)std::floor(p.X + f.x / 2.0f), y = (int)std::floor(p.Y), z = (int)std::floor(p.Z + f.z / 2.0f);
		if (x < 0 || y < 0 || z < 0 || x >= f.x || y >= f.y || z >= f.z)
			continue;
		const size_t i = ((size_t)y * f.z + z) * f.x + x;
		const sandbox::Stage::Cell &c = f.cells[i];
		const bool air = c.block < 0 || !m_blocks || (*m_blocks)[c.block].spec.matter == "air";
		if (!air || (c.fill.substance >= 0 && c.fill.share > 0.005) || (c.main >= 0 && c.filled > 0.005) || (c.vapour >= 0 && c.haze > 0.02))
			return (int)i;
	}
	return std::nullopt;
}

bool Scene::handle(const SEvent &event)
{
	if (!m_shown || event.EventType != EET_MOUSE_INPUT_EVENT)
		return false;
	const v2s32 at(event.MouseInput.X, event.MouseInput.Y);
	m_over = m_rect.isPointInside(at);
	switch (event.MouseInput.Event) {
	case EMIE_LMOUSE_PRESSED_DOWN:
		if (!m_over)
			return false;
		m_dragging = true;
		m_mouse = at;
		return true;
	case EMIE_LMOUSE_LEFT_UP: {
		const bool was = m_dragging;
		m_dragging = false;
		return was;
	}
	case EMIE_MOUSE_MOVED:
		if (m_dragging) {
			m_yaw += (at.X - m_mouse.X) * 0.008f;
			m_pitch = std::clamp(m_pitch + (at.Y - m_mouse.Y) * 0.008f, 0.02f, 1.45f);
		}
		m_mouse = at;
		return m_dragging || m_over;
	case EMIE_MOUSE_WHEEL:
		if (!m_over)
			return false;
		m_distance = std::clamp(m_distance * std::pow(0.9f, event.MouseInput.Wheel), 1.5f, 80.f);
		return true;
	default:
		return false;
	}
}

std::optional<int> Scene::takeHover()
{
	const int now = pick().value_or(-1);
	if (now == m_hover)
		return std::nullopt;
	m_hover = now;
	return now;
}

} // namespace shell
