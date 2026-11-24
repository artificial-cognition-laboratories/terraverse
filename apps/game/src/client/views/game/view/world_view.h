// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_ptr.h"
#include "src/shared/base/irr_v3d.h"
#include <SColor.h>

class Camera;
class Client;
class Clouds;
class IWritableShaderSource;
class IWritableTextureSource;
class RenderingEngine;
class Sky;
struct ClientEvent;
struct MapDrawControl;
namespace far { class View; }
namespace liquids { class Layer; }
namespace planetarium { class Sky; }

/*
	The world as it's seen (sprint 126): the sky and the planet's system round it, the far view to
	the horizon, the water lying in the world, the clouds, the light of the time of day, fog, and
	the scene drawn. What the game shows, out of Game, so a body's eye (client/app/headless) sees
	exactly what a player does.

	It owns what's drawn besides the map and its objects, takes the server's word on the sky (its
	client events), and each frame works out what changed. Game and an eye both call it the same
	way: `heard` each event of the sky, `step` each frame, then within the frame `fog` and `draw`.
*/
class WorldView
{
public:
	struct Opts
	{
		Client &client;
		Camera &camera;
		RenderingEngine &engine;
		IWritableShaderSource &shaders;
		IWritableTextureSource &textures;
		// How far the map's drawn: the sky's fog can pull it in (a server's fog distance).
		MapDrawControl &draw;
	};
	explicit WorldView(Opts o);
	~WorldView();
	WorldView(const WorldView &) = delete;
	WorldView &operator=(const WorldView &) = delete;

	// What one frame knows that the view doesn't.
	struct Frame
	{
		// Flying through things (noclip and fly): the sky's as bright as above ground, to see by.
		bool see_through;
		// The camera's offset moved this frame: the map's draw list is made again.
		bool offset_moved;
	};

	// The server's word on the sky (sky, sun, moon, stars, clouds, the day's light): true when the
	// event was one of these, applied and let go; false when it's another's.
	bool heard(ClientEvent &event);
	// What of those changes the world itself, not how it's drawn: the real sun and the day's
	// light. A blind body, which draws nothing, applies only this; `heard` applies it too.
	static void light(Client &client, const ClientEvent &event);
	// One frame, `dtime` s: the planet's sky and far view, water, the light, clouds, particles,
	// the map's draw list, shadows.
	void step(float dtime, const Frame &frame);
	// The camera's offset moved: the clouds with it.
	void offsetMoved(v3s16 offset);

	// Within a frame, before anything's drawn: the fog as the sky and the far view want it.
	void fog();
	// What a frame's cleared to.
	video::SColor skyColor() const;
	// The world drawn into the frame begun: what the game shows, with or without its HUD.
	void draw(bool show_hud, bool wield, bool crosshair);

	// The game's sky (what the world's shaders read its sun and moon from).
	Sky &sky() { return *m_sky; }
	// Whether the fog's drawn: the player's choice, unless the server sets a fog distance.
	bool fogEnabled() const;

private:
	void ensurePlanet();
	void updateClouds(float dtime);
	void updateShadows();

	Client &m_client;
	Camera &m_camera;
	RenderingEngine &m_engine;
	IWritableShaderSource &m_shaders;
	IWritableTextureSource &m_textures;
	MapDrawControl &m_draw;

	irr_ptr<Clouds> m_clouds;
	irr_ptr<Sky> m_sky;
	irr_ptr<planetarium::Sky> m_space; // the system round the planet, behind the far view
	irr_ptr<far::View> m_far;          // the horizon, on a terra world
	irr_ptr<liquids::Layer> m_liquids; // liquid in the world's open space (sprint 81)
	int m_far_body = 0;                // the atlas body the far view draws

	float m_fog_range = 0;
	float m_time_of_day_smooth = 0;
	float m_update_draw_list_timer = 0;
	float m_touch_blocks_timer = 0;
	v3f m_update_draw_list_last_cam_dir;
};
