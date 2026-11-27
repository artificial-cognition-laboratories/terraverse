// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes_bloated.h"
#include <chrono>
#include <unordered_map>

class Client;
class MapSector;
namespace video { class IVideoDriver; }

/*
	The loaded blocks' meshes kept on the card: every few seconds of the clock
	each one's buffers there alone are marked used (MapBlockMesh::touch), so the
	driver never lets one go, and one it let go anyway is made again. What they
	hold is reported as the map's own (the debugger's Memory tab).
*/
class NearMeshes
{
public:
	struct Opts
	{
		Client &client; // makes a lost mesh again
	};
	explicit NearMeshes(Opts o);

	// The meshes of the sectors' blocks kept, if it's time.
	void keep(const std::unordered_map<v2s16, MapSector *> &sectors, video::IVideoDriver *driver);

private:
	Client &m_client;
	std::chrono::steady_clock::time_point m_kept{};
};
