// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irr_v3d.h"
#include <optional>

class Server;

/*
	A river's surface on the server (sprint 122, R2): its held water's top node
	at its part with its river's rise, blended between map blocks as the
	clients blend it (fill::surface), so the world's matter holds a river
	where everyone sees and swims in it.
*/
// How full a node's held water is, sixteenths over its floor: under its
// river's top node, 16; its top node, at the surface. Nothing: not held water.
std::optional<int> terraHeldSurface(Server *server, v3s16 node);
