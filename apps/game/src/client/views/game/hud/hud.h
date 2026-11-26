// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
// Copyright (C) 2017 red-001 <red-001@outlook.ie>

#pragma once

#include <array>

#include <vector>
#include <IGUIFont.h>
#include <SMaterial.h>
#include <CMeshBuffer.h>
#include "irr_ptr.h"
#include "src/shared/base/irr_aabb3d.h"
#include "src/shared/game/objects/hud_element.h"

class Client;
class ITextureSource;
class Inventory;
class InventoryList;
class LocalPlayer;
struct ItemStack;

namespace scene
{
	class IMesh;
}

namespace video
{
	class ITexture;
	class IVideoDriver;
}

class Hud
{
public:
	enum BlockBoundsMode
	{
		BLOCK_BOUNDS_OFF,
		BLOCK_BOUNDS_CURRENT,
		BLOCK_BOUNDS_NEAR,
	} m_block_bounds_mode = BLOCK_BOUNDS_OFF;

	video::SColor crosshair_argb;
	video::SColor selectionbox_argb;

	bool use_crosshair_image = false;
	bool use_object_crosshair_image = false;
	std::string hotbar_image = "";
	bool use_hotbar_image = false;
	std::string hotbar_selected_image = "";
	bool use_hotbar_selected_image = false;

	bool pointing_at_object = false;

	Hud(Client *client, LocalPlayer *player,
			Inventory *inventory);
	void readScalingSetting();
	~Hud();

	enum BlockBoundsMode toggleBlockBounds();
	void disableBlockBounds();
	void drawBlockBounds();

	void drawHotbar(const v2s32 &pos, const v2f &offset, u16 direction, const v2f &align);
	void resizeHotbar();
	void drawCrosshair();
	void drawSelectionMesh();
	void updateSelectionMesh(const v3s16 &camera_offset);

	std::vector<aabb3f> *getSelectionBoxes() { return &m_selection_boxes; }
	// Terraverse, an atlas: the pointed cell's outline as it's drawn (scene
	// coordinates: AtlasView::outline), in place of the boxes. Empty: the boxes.
	std::vector<std::pair<v3f, v3f>> *getSelectionLines() { return &m_selection_lines; }
	void setSelectionThrough(bool through) { m_selection_through = through; }
	// Terraverse: lines over the world (the camera offset taken off), each
	// its colour: a held object's turning handle
	struct Guide
	{
		v3f a, b;
		video::SColor colour;
	};
	std::vector<Guide> *getGuides() { return &m_guides; }
	// Terraverse: the world's block grid in build mode (sprint 73): lines
	// as guides are, their colours' alpha blended (they fade out)
	std::vector<Guide> *getGrid() { return &m_grid; }
	// Terraverse: words at a place in the world (BS, the world's), drawn
	// over it (sprint 63: the joint menu's)
	struct Label
	{
		v3f at;
		std::wstring text;
		video::SColor colour;
	};
	std::vector<Label> *getLabels() { return &m_labels; }
	// Terraverse: a button in the world (sprint 71: the part menu), drawn
	// as the page's are (a stone face, lit from the top left, the accent
	// under it when lit) with its sign's strokes on it, over the world
	// (never in the ground). BS, the camera offset taken off: its middle
	// and half its width and height along its plane.
	struct Tile
	{
		v3f centre, u, w;
		bool lit;
		std::vector<std::pair<v2f, v2f>> strokes; // in its halves, -1..1
	};
	std::vector<Tile> *getTiles() { return &m_tiles; }
	// Terraverse: a box seen through, where something would go (sprint 123:
	// a member held): its corners in the scene (BS, the camera offset taken
	// off; bit 0 x, bit 1 y, bit 2 z: max), its colour (its alpha the fill's).
	struct Solid
	{
		std::array<v3f, 8> corners;
		video::SColor colour;
	};
	std::vector<Solid> *getSolids() { return &m_solids; }

	void setSelectionPos(const v3f &pos, const v3s16 &camera_offset);

	v3f getSelectionPos() const { return m_selection_pos; }

	void setSelectionRotationRadians(v3f rotation)
	{
		m_selection_rotation_radians = rotation;
	}

	v3f getSelectionRotationRadians() const
	{
		return m_selection_rotation_radians;
	}

	void setSelectionMeshColor(const video::SColor &color)
	{
		m_selection_mesh_color = color;
	}

	void setSelectedFaceNormal(const v3f &face_normal)
	{
		m_selected_face_normal = face_normal;
	}

	bool hasElementOfType(HudElementType type);

	void drawLuaElements(const v3s16 &camera_offset, bool only_unhidable);

private:
	bool calculateScreenPos(const v3s16 &camera_offset, HudElement *e, v2s32 *pos);
	void drawStatbar(v2s32 pos, u16 corner, u16 drawdir,
			const std::string &texture, const std::string& bgtexture,
			s32 count, s32 maxcount, v2s32 offset, v2f size = v2f());

	void drawItems(v2s32 screen_pos, v2s32 screen_offset, s32 itemcount, v2f alignment,
			s32 inv_offset, InventoryList *mainlist, u16 selectitem,
			u16 direction, bool is_hotbar, const std::string &background_image = "");

	void drawItem(const ItemStack &item, const core::rect<s32> &rect, bool selected);

	void drawCompassTranslate(HudElement *e, video::ITexture *texture,
			const core::rect<s32> &rect, int way);

	void drawCompassRotate(HudElement *e, video::ITexture *texture,
			const core::rect<s32> &rect, int way);

	Client *client = nullptr;
	video::IVideoDriver *driver = nullptr;
	LocalPlayer *player = nullptr;
	Inventory *inventory = nullptr;
	ITextureSource *tsrc = nullptr;

	float m_hud_scaling; // cached setting
	float m_scale_factor;
	v3s16 m_camera_offset;
	v2u32 m_screensize;
	v2s32 m_displaycenter;
	s32 m_hotbar_imagesize; // Takes hud_scaling into account, updated by resizeHotbar()
	s32 m_padding; // Takes hud_scaling into account, updated by resizeHotbar()
	video::SColor hbar_colors[4];

	std::vector<aabb3f> m_selection_boxes;
	std::vector<std::pair<v3f, v3f>> m_selection_lines; // Terraverse
	bool m_selection_through = false; // Terraverse: the lines seen through what's in front, dim
	std::vector<Guide> m_guides; // Terraverse
	std::vector<Guide> m_grid;   // Terraverse
	std::vector<Label> m_labels; // Terraverse
	std::vector<Tile> m_tiles; // Terraverse
	std::vector<Solid> m_solids; // Terraverse
	void drawSolids(video::IVideoDriver *driver); // Terraverse
	void drawTiles(video::IVideoDriver *driver); // Terraverse
	void drawLabels(const v3s16 &camera_offset); // Terraverse
	std::vector<aabb3f> m_halo_boxes;
	v3f m_selection_pos;
	v3f m_selection_pos_with_offset;
	v3f m_selection_rotation_radians;

	scene::IMesh *m_selection_mesh = nullptr;
	video::SColor m_selection_mesh_color;
	v3f m_selected_face_normal;

	video::SMaterial m_selection_material;
	video::SMaterial m_block_bounds_material;

	irr_ptr<scene::SMeshBuffer> m_rotation_mesh_buffer;

	enum
	{
		HIGHLIGHT_BOX,
		HIGHLIGHT_HALO,
		HIGHLIGHT_NONE
	} m_mode;
};
