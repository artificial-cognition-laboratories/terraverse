// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>

namespace terragen { struct SystemSpec; }

namespace shell {

/*
	The `system.preview` verb: a system's causes from the create screen in,
	every body's card and history out (terragen::bodies). The page holds
	only causes; everything it shows about a body comes from here. Throws
	Refused on causes the page shouldn't have been able to send.

	  { star, age, activity, bodies: [ { id, parent, orbit, eccentricity,
	    mass, formed, volatiles, day, life } ] }
*/
Json::Value systemPreview(const Json::Value &intent);

/*
	A system as the page holds one (ui/src/system/model.ts: Body, with the
	star's id, age and activity): the engine's spec is the one source, the
	page's draft starts from it (`system.home`, SystemSpec::home) and the
	game's map shows it (`system.world`).
*/
Json::Value systemJson(const terragen::SystemSpec &spec);
// `system.world`: { code } in; the world's system (SystemSpec::forWorld) and
// every body's card out ({ star, age, activity, bodies, cards }).
Json::Value worldSystem(const Json::Value &intent);

} // namespace shell
