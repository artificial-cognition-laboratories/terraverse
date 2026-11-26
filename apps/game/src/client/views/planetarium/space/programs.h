// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/views/planetarium/globe/lit.h"
#include "src/client/views/planetarium/globe/rim.h"
#include "src/client/views/planetarium/space/galaxy.h"

class IShaderSource;

namespace planetarium {

/*
	A system's shaders, compiled once and handed on: a view of another world
	takes over the one it replaces (Planetarium::keep). Made a view each, the
	driver compiled them all again at its first draw, tens of ms on the frame,
	each time the menu went to Play or back. What's per view (the light, a
	rim's textures) is set before each draw, so one serves any number.
*/
class Programs
{
public:
	explicit Programs(IShaderSource *source);

	Lit lit;
	Rim::Program rim;
	Galaxy galaxy;
};

} // namespace planetarium
