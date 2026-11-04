// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "networkprotocol.h"


/*
	PROTOCOL VERSION < 37:
		Until (and including) version 0.4.17.1
	PROTOCOL VERSION 37:
		Redo detached inventory sending
		Add TOCLIENT_NODEMETA_CHANGED
		New network float format
		ContentFeatures version 13
		Add full Euler rotations instead of just yaw
		Add TOCLIENT_PLAYER_SPEED
		[bump for 5.0.0]
	PROTOCOL VERSION 38:
		Incremental inventory sending mode
		Unknown inventory serialization fields no longer throw an error
		Mod-specific formspec version
		Player FOV override API
		"ephemeral" added to TOCLIENT_PLAY_SOUND
	PROTOCOL VERSION 39:
		Updated set_sky packet
		Adds new sun, moon and stars packets
		Minimap modes
	PROTOCOL VERSION 40:
		TOCLIENT_MEDIA_PUSH changed, TOSERVER_HAVE_MEDIA added
	PROTOCOL VERSION 41:
		Added new particlespawner parameters
		[scheduled bump for 5.6.0]
	PROTOCOL VERSION 42:
		TOSERVER_UPDATE_CLIENT_INFO added
		new fields for TOCLIENT_SET_LIGHTING and TOCLIENT_SET_SKY
		Send forgotten TweenedParameter properties
		[scheduled bump for 5.7.0]
	PROTOCOL VERSION 43:
		"start_time" added to TOCLIENT_PLAY_SOUND
		place_param2 type change u8 -> optional<u8>
		[scheduled bump for 5.8.0]
	PROTOCOL VERSION 44:
		AO_CMD_SET_BONE_POSITION extended
		Add TOCLIENT_MOVE_PLAYER_REL
		Move default minimap from client-side C++ to server-side builtin Lua
		[scheduled bump for 5.9.0]
	PROTOCOL VERSION 45:
		Minimap HUD element supports negative size values as percentages
		[bump for 5.9.1]
	PROTOCOL VERSION 46:
		Move default hotbar from client-side C++ to server-side builtin Lua
		Add shadow tint to Lighting packets
		Add shadow color to CloudParam packets
		Move death screen to server and make it a regular formspec
			The server no longer triggers the hardcoded client-side death
			formspec, but the client still supports it for compatibility with
			old servers.
		Rename TOCLIENT_DEATHSCREEN to TOCLIENT_DEATHSCREEN_LEGACY
		Rename TOSERVER_RESPAWN to TOSERVER_RESPAWN_LEGACY
		Support float animation frame numbers in TOCLIENT_LOCAL_PLAYER_ANIMATIONS
		[scheduled bump for 5.10.0]
	PROTOCOL VERSION 47
		Add particle blend mode "clip"
		[scheduled bump for 5.11.0]
	PROTOCOL VERSION 48
		Add compression to some existing packets
		[scheduled bump for 5.12.0]
	PROTOCOL VERSION 49
		Support of showing a player inventory using 'core.show_formspec'
		[scheduled bump for 5.13.0]
	PROTOCOL VERSION 50
		Support for TOCLIENT_SPAWN_PARTICLE_BATCH
		[scheduled bump for 5.14.0]
	PROTOCOL VERSION 51
		Only send first frame of animated item/wield images to older client
		[scheduled bump for 5.15.0]
	PROTOCOL VERSION 52
		Added "skip_wield_anim" to TOCLIENT_INVENTORY
		Type of TOCLIENT_HUDADD `size` changed from v2s32 to v2f
		[scheduled bump for 5.16.0]
	PROTOCOL VERSION 53
		Added optional "do_interpolate_rotation" u8 to AO_CMD_UPDATE_POSITION
		[scheduled bump for 5.17.0]
	PROTOCOL VERSION 54
		Send 'en' translation files to clients as a fallback
		[scheduled bump for 5.18.0]
	PROTOCOL VERSION 55 (Terra Firma)
		TOCLIENT_UI, TOSERVER_UI: the page's UI channel
		TOCLIENT_SET_SKY: trailing solar { on, latitude, declination }
	PROTOCOL VERSION 56 (Terra Firma)
		TOCLIENT_HORIZON: the world code and biome blocks, for the far view
	PROTOCOL VERSION 57 (Terra Firma)
		TOCLIENT_ALOFT: where the player is over the planet, from space
	PROTOCOL VERSION 58 (Terra Firma)
		AUTH_MECHANISM_TICKET, TOSERVER_TICKET: friends join with an ArcLabs
		ticket (src/online/gate.h), and must run the host's build
	PROTOCOL VERSION 59 (Terra Firma)
		TOCLIENT_PLANET: the world is a planet, and its size
	PROTOCOL VERSION 60 (Terra Firma)
		TOCLIENT_ALOFT gone (space bubbles: orbit is flight over the planet)
	PROTOCOL VERSION 61 (Terra Firma)
		TOCLIENT_PLANET carries the world's sphere mapping version
	PROTOCOL VERSION 62 (Terra Firma)
		TOCLIENT_PLANET gone (the cube-sphere net: every planet is an atlas)
	PROTOCOL VERSION 63 (Terra Firma)
		TOCLIENT_WINDOW: the planet's tiles in the map (planet::Atlas::window)
	PROTOCOL VERSION 64 (Terra Firma)
		TOCLIENT_ATLAS: every body's code; TOCLIENT_WINDOW: a tile's body
	PROTOCOL VERSION 65 (Terra Firma)
		Map format 30: cut nodes' shapes; TOSERVER_SCULPT, TOSERVER_UNDO, TOCLIENT_SHAPE
	PROTOCOL VERSION 66 (Terra Firma)
		Pieces: object property `piece`, visual "piece"; TOSERVER_SCULPT
		names a piece (object id) or the world (0); TOSERVER_PIECE_NEW
	PROTOCOL VERSION 67 (Terra Firma)
		A piece's text is TFP2 (an id, ghost or real); TOSERVER_BLUEPRINT_SAVE,
		TOSERVER_BLUEPRINT_PLACE, TOSERVER_PIECE_REAL, TOCLIENT_BLUEPRINTS
	PROTOCOL VERSION 68 (Terra Firma)
		Objects: the `piece` property is an object's text (TFO1: design and
		built); TOSERVER_PIECE_NEW has no position; TOSERVER_PIECE_REAL gone,
		TOSERVER_OBJECT_HOLD, TOSERVER_OBJECT_TURN, TOCLIENT_HELD
	PROTOCOL VERSION 69 (Terra Firma)
		TOSERVER_OBJECT_TURN gone, TOSERVER_OBJECT_POSE: the holder poses it;
		TOSERVER_SCULPT's footprint dragged; TOSERVER_HOLLOW. Machines (TFM1):
		TOSERVER_SCULPT and TOSERVER_HOLLOW name a part, TOSERVER_SCULPT acts
		(cut, add, paint) and grows parts; TOSERVER_LINK, TOSERVER_JOIN,
		TOSERVER_PART; sockets (TFM3): TOSERVER_SOCKET, TOSERVER_PLUG
	PROTOCOL VERSION 70 (Terra Firma)
		Machines move by physics (sprint 57): AO_CMD_SET_PARTS, where each of a
		machine's rigid groups is in its first's (a wheel's turn)
	PROTOCOL VERSION 71 (Terra Firma)
		The player's body (sprint 58): TOCLIENT_MATERIALS; TOSERVER_OBJECT_HOLD's
		u8 is how it's held: 0 let go, 1 held still (the bench), 2 carried
	PROTOCOL VERSION 72 (Terra Firma)
		Pushing (sprint 61): TOSERVER_OBJECT_PUSH, what a player's body gave
		an object it walked into (theirs is held where the server has it)
	PROTOCOL VERSION 73 (Terra Firma)
		Joints (sprint 63): a joint's u8 is 0 fixed, 1 rotary (free: what
		was "hinge"), 2 hinge (between stops)
	PROTOCOL VERSION 74 (Terra Firma)
		Fitting (sprint 64): TOSERVER_JOIN and TOSERVER_PLUG end in a u8
		spin, quarter turns about the joining axis; TOSERVER_PLUG_AGAIN
	PROTOCOL VERSION 75 (Terra Firma)
		Parts (sprint 65): a round stroke's footprint is dragged from its
		middle out (sculpt::Stroke::cells)
	PROTOCOL VERSION 76 (Terra Firma)
		Grabbing (sprint 68): TOSERVER_OBJECT_GRAB
	PROTOCOL VERSION 77 (Terra Firma)
		Driving (sprint 69): TOCLIENT_MATERIALS gives each its role and
		torque; TOSERVER_DRIVE
	PROTOCOL VERSION 78 (Terra Firma)
		Build mode (sprint 70): a machine's text is TFM4, ending in its
		front
	PROTOCOL VERSION 79 (Terra Firma)
		Vehicles (sprint 73): joints that steer and are driven (new
		numbers where a joint's kind is sent); TOSERVER_DRIVE has no
		facing (the seat's front is ahead)
	PROTOCOL VERSION 80 (Terra Firma)
		Copy and paste (sprint 74): TOSERVER_CLIPBOARD; TOSERVER_BLUEPRINT_SAVE
		names a part; TOCLIENT_HELD says whether it's to be carried
	PROTOCOL VERSION 81 (Terra Firma)
		Suspension (sprint 75): a machine's text is TFM5 (each joint's
		spring), the sprung joint; TOSERVER_LINK ends in the spring;
		TOSERVER_HOLLOW ends in rounded; frame, truss and beam tools
	PROTOCOL VERSION 82 (Terra Firma)
		Parts (sprint 76): a sprung joint's hub is a body of its own (the
		rigid groups changed); a machine's text may end in its plug
	PROTOCOL VERSION 83 (Terra Firma)
		Assembly (sprint 77): a plug may end in the face it hung by;
		TOSERVER_CLIPBOARD pulls a part off; TOCLIENT_HELD says how it's handed
	PROTOCOL VERSION 84 (Terra Firma)
		Engines (sprint 78): TOCLIENT_MATERIALS gives a motor's power and
		rpm, not its torque; TOSERVER_UPRIGHT
	PROTOCOL VERSION 85 (Terra Firma)
		Flow (sprint 80): the player's keys carry pour (bit 10)
	PROTOCOL VERSION 86 (Terra Firma)
		The liquid layer (sprint 81): map format 31 (fills); TOCLIENT_FILL
	PROTOCOL VERSION 87 (Terra Firma)
		Fine water (sprint 82): TOCLIENT_STREAM, liquid poured
	PROTOCOL VERSION 88 (Terra Firma)
		Ground (sprints 84-85): TOCLIENT_SHAPE's shape is kept as sent, a
		whole one too (frozen ground); its flag is the node uncut
	PROTOCOL VERSION 89 (Terra Firma)
		The world on the sphere (sprint 86): TOCLIENT_HORIZON's blocks carry
		no ice (the polar wall is gone); its code is TF6
	PROTOCOL VERSION 90 (Terra Firma)
		A body's clock (sprint 107): TOCLIENT_SET_SKY's solar carries turns and at
		Only 90 is accepted on either side.
*/

// Note: Also update core.protocol_versions in builtin when bumping
const u16 LATEST_PROTOCOL_VERSION = 97;

// See also formspec [Version History] in doc/lua_api.md
const u16 FORMSPEC_API_VERSION = 12;
