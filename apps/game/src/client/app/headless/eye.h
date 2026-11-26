// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>
#include <memory>
#include <optional>
#include <string>

class Camera;
class Client;
class GameGlobalShaderUniformSetterFactory;
class Hud;
class IWritableShaderSource;
class IWritableTextureSource;
class RenderingEngine;
class WorldView;
struct ClientEvent;
struct MapDrawControl;

namespace headless {

/*
	A body's eye (sprint 126): the world drawn as the game draws it, by the game's own view
	(views/game/view/world_view.h: the planet's sky, the far view, water, clouds, light, fog),
	from the body's head, into a window nobody sees (SDL's offscreen driver: a GL context by EGL,
	no display). Each picture is a PNG, 30 a second, in a ring of files in shared memory (/dev/shm:
	memory, not disk), and its reading says where: the mind's body module keeps the file a reading
	names (arclabs, registry/modules/body).
*/
class Eye
{
public:
	// Its picture's size, px.
	struct Size
	{
		unsigned width, height;
	};
	struct Opts
	{
		Client &client;
		RenderingEngine &engine;
		IWritableShaderSource &shaders;
		IWritableTextureSource &textures;
		Camera &camera;
		// What the world's shaders read each frame (added to the shaders before the content).
		GameGlobalShaderUniformSetterFactory &uniforms;
		// Where its pictures are kept: a folder of its own.
		std::string dir;
		Size size;
		// How far the map's drawn (the server's fog can pull it in).
		MapDrawControl &draw;
	};
	explicit Eye(Opts o);
	~Eye();

	// The sense, as the manifest declares it.
	Json::Value declared() const;
	// The server's word on the sky: true when it was the view's.
	bool heard(ClientEvent &event);
	// One step of the world as it's seen; a picture, when one's due at `now` (ms): its reading {ch, at, ref}.
	std::optional<Json::Value> see(unsigned long long now, float dtime);

private:
	Client &m_client;
	RenderingEngine &m_engine;
	Camera &m_camera;
	std::string m_dir;
	Size m_size;
	std::unique_ptr<Hud> m_hud;
	std::unique_ptr<WorldView> m_view;
	unsigned long long m_next = 0;
	unsigned m_taken = 0;
};

} // namespace headless
