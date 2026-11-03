// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2016 est31, <MTest31@outlook.com>

#pragma once

class KeyType
{
public:
	enum T
	{
		// Player movement
		FORWARD,
		BACKWARD,
		LEFT,
		RIGHT,
		JUMP,
		AUX1,
		SNEAK,
		AUTOFORWARD,
		DIG,
		PLACE,

		ESC,

		// Camera look (keyboard)
		CAMERA_YAW_LEFT,
		CAMERA_YAW_RIGHT,
		CAMERA_PITCH_UP,
		CAMERA_PITCH_DOWN,

		// Other
		DROP,
		INVENTORY,
		LAB, // Terra Firma
		WIKI, // Terra Firma
		PLANETARIUM, // Terra Firma
		TOOLS, // Terra Firma: the tools in hand out or away
		PRONE, // Terra Firma: lie down (sprint 116)
		BRAKE, // Terra Firma: flight's brake (sprint 59)
		POUR, // Terra Firma: pour what's in hand (sprint 80)
		TOOL_NEXT, // Terra Firma
		FOCUS, // Terra Firma: the focus view
		MIRROR, // Terra Firma
		UNDO, // Terra Firma
		TURN_LEFT, TURN_RIGHT, // Terra Firma: what's carried turned a quarter (sprint 64)
		CURSOR, // Terra Firma: the cursor freed, and taken back
		COPY, PASTE, SAVE_PART, // Terra Firma: with Ctrl (aux1) while building: a part copied, pasted, kept (sprint 74)
		ALT, // Terra Firma: the build layer: with it, the slot keys pick shapes, = mirrors, a click carries (sprint 68)
		CHAT,
		CMD,
		CMD_LOCAL,
		CONSOLE,
		MINIMAP,
		FREEMOVE,
		PITCHMOVE,
		FASTMOVE,
		NOCLIP,
		HOTBAR_PREV,
		HOTBAR_NEXT,
		MUTE,
		INC_VOLUME,
		DEC_VOLUME,
		CINEMATIC,
		SCREENSHOT,
		TOGGLE_BLOCK_BOUNDS,
		TOGGLE_HUD,
		TOGGLE_CHAT,
		TOGGLE_FOG,
		TOGGLE_UPDATE_CAMERA,
		TOGGLE_DEBUG,
		TOGGLE_PROFILER,
		CAMERA_MODE,
		INCREASE_VIEWING_RANGE,
		DECREASE_VIEWING_RANGE,
		RANGESELECT,
		ZOOM,

		QUICKTUNE_NEXT,
		QUICKTUNE_PREV,
		QUICKTUNE_INC,
		QUICKTUNE_DEC,

		// hotbar
		SLOT_1,
		SLOT_2,
		SLOT_3,
		SLOT_4,
		SLOT_5,
		SLOT_6,
		SLOT_7,
		SLOT_8,
		SLOT_9,
		SLOT_10,
		SLOT_11,
		SLOT_12,
		SLOT_13,
		SLOT_14,
		SLOT_15,
		SLOT_16,
		SLOT_17,
		SLOT_18,
		SLOT_19,
		SLOT_20,
		SLOT_21,
		SLOT_22,
		SLOT_23,
		SLOT_24,
		SLOT_25,
		SLOT_26,
		SLOT_27,
		SLOT_28,
		SLOT_29,
		SLOT_30,
		SLOT_31,
		SLOT_32,

		// Fake keycode for array size and internal checks
		INTERNAL_ENUM_COUNT

	};
};

typedef KeyType::T GameKeyType;
