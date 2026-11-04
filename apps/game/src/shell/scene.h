// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irrlichttypes_bloated.h"
#include "physics/skeleton.h"
#include "physics/world.h"
#include "finewater.h"
#include "sandbox/stage.h"
#include <IEventReceiver.h>
#include <S3DVertex.h>
#include <matrix4.h>
#include <rect.h>
#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace video { class ITexture; class IVideoDriver; }

namespace shell {

/*
	A small simulation drawn natively where the page leaves room for it, with
	the page's panels floating over it, as the Planetarium is: a few blocks of
	the sandbox, seen from a camera the mouse turns. Drawn under the page every
	frame; its look only rebuilt when the simulation has moved on. Knows
	nothing about what's in it beyond what a sandbox says of each block.
*/
class Scene
{
public:
	~Scene();

	// A sandbox's state, as stepping left it (from any thread's copy: Wiki's).
	struct Frame
	{
		int32_t x, y, z;
		std::vector<sandbox::Stage::Cell> cells;
		double time = -1; // the matter's clock; none on a course, whose liquid isn't run
		std::vector<std::pair<sandbox::Pos, size_t>> pouring;
	};
	// Where it draws (window pixels). A new size reframes the camera.
	// `right`: window pixels of it covered from the right; the blocks are centred in the rest.
	void place(core::recti rect, float right) { m_rect = rect; m_right = right; }
	void frame(Frame frame);
	// The game's blocks, as a Stage's cells name them, and each substance's colour (for what's loose in a block).
	void looks(const std::vector<sandbox::StageBlock> *blocks, std::vector<video::SColor> substances, std::vector<bool> water)
	{
		m_blocks = blocks;
		m_substances = std::move(substances);
		m_water = std::move(water);
		m_dirty = true;
	}
	void heat(bool on) { m_heat = on; m_dirty = true; }
	// A body in it, its feet here (scene's frame), and the camera on it.
	struct Figure
	{
		physics::Bones bones;
		v3f at;
		float yaw;
	};
	void figure(std::optional<Figure> figure);
	// A machine in it: what its bodies look like (their frame, nodes), once; where they are, each frame.
	struct LookBox
	{
		aabb3f box;
		std::array<std::string, 3> tiles;
	};
	using Looks = std::vector<std::vector<LookBox>>;
	struct Machine
	{
		struct Root { v3f at; physics::Quat turn; } root;
		std::vector<physics::Relative> bodies; // each but the first, against it
	};
	void machine(Looks looks) { m_looks = std::move(looks); m_parts.clear(); m_built_parts = false; }
	void machine(std::optional<Machine> at) { m_machine = std::move(at); }
	void follow(v3f at);
	// Loose ground in it, in sixteenth columns from here (sixteenths of height). Remeshed when `version` moves on.
	struct Ground
	{
		v3f corner;
		int width = 0, depth = 0;
		std::vector<int> top;
		std::array<std::string, 3> tiles;
		u64 version = 0;
		// the wiki's land: flat colours by column (none: its tiles), and how big a column and a unit of height are
		std::vector<u32> colours;
		float cell = 1.f / 16, rise = 1.f / 16;
		int floor = 0; // what its sides stop at
	};
	void ground(std::optional<Ground> ground);
	void distance(float d) { m_distance = d; }
	void hide();
	bool shown() const { return m_shown; }
	void show() { m_shown = true; }

	void draw(video::IVideoDriver *driver);
	// The mouse (what the page left alone): drag turns, the wheel zooms. Whether it was ours.
	bool handle(const SEvent &event);
	// The block under the mouse, once it's changed: its index, or −1. None: no change.
	std::optional<int> takeHover();

private:
	void rebuild();
	void drawFigure(video::IVideoDriver *driver);
	// a texture's vertices, in as many buffers as its 16-bit indices need
	struct Batch
	{
		video::ITexture *texture; // none: flat colour
		bool clear;
		float frames; // of the texture's height, one tile (an animation's strip: its first frame)
		std::vector<video::S3DVertex> vertices;
		std::vector<u16> indices;
	};
	Batch &batch(video::ITexture *texture, bool clear);
	video::ITexture *texture(const std::string &name);
	// a box's faces, from their tiles: top, bottom, side (a game's tile string: a file, maybe a multiply)
	void addBox(const std::array<std::string, 3> &tiles, bool clear, v3f middle, v3f size, video::SColor light);
	void addPlain(video::ITexture *texture, bool clear, v3f middle, v3f size, video::SColor colour);
	std::optional<int> pick() const;
	core::matrix4 view() const;

	bool m_shown = false, m_heat = false, m_dirty = false;
	core::recti m_rect;
	float m_right = 0;
	v2f shift() const;
	std::optional<Frame> m_frame;
	const std::vector<sandbox::StageBlock> *m_blocks = nullptr;
	std::vector<video::SColor> m_substances;
	std::vector<bool> m_water; // by substance: draws as the game's water
	std::vector<Batch> m_batches;
	std::map<std::string, video::ITexture *> m_textures; // the driver's; kept for it
	struct Flame { v3f at; float strength; };
	std::vector<Flame> m_flames;
	video::IVideoDriver *m_driver = nullptr;
	int32_t m_sizedX = 0, m_sizedY = 0, m_sizedZ = 0;
	std::optional<Figure> m_figure;
	void drawFine(video::IVideoDriver *driver, float dt);
	FineWater m_fine;
	bool m_fine_on = false;
	double m_fine_time = -1;
	float m_fine_pace = 1;
	u64 m_fine_at = 0;
	struct Drop { v3f at, speed; video::SColor colour; };
	std::vector<Drop> m_drops;
	std::vector<video::S3DVertex> m_fine_vertices;
	std::vector<u16> m_fine_indices;
	void drawMachine(video::IVideoDriver *driver);
	void buildGround();
	std::optional<Ground> m_ground;
	std::vector<Batch> m_ground_batches;
	bool m_ground_dirty = false;
	void drawBatches(video::IVideoDriver *driver, const std::vector<Batch> &batches, bool clear);
	Looks m_looks;
	std::vector<std::vector<Batch>> m_parts; // by body
	bool m_built_parts = false;
	std::optional<Machine> m_machine;
	v3f m_target; // what the camera turns about
	v3f eye() const;
	float m_yaw = 0.7f, m_pitch = 0.5f, m_distance = 6;
	bool m_dragging = false;
	v2s32 m_mouse;
	bool m_over = false;
	int m_hover = -1;
	std::optional<int> m_hoverNews;
	u32 m_ticks = 0;
};

} // namespace shell
