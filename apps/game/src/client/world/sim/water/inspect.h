// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes.h"
#include <json/json.h>
#include <optional>
#include <string>

namespace liquids {

class Grid;
class Base;
class Flat;

// What the water's drawn as: itself, or something about it (the debugger's World tab)
enum class View : u8 { Water, Disturbance, Kinds, Running };

// How the water's shown and run, as the debugger's set it
struct Look
{
	View view = View::Water;
	bool grid = true;    // the grid's water drawn
	bool flat = true;    // the flat water drawn
	bool frozen = false; // the disturbance and the levels' easing stopped where they are
	bool rise = true;    // a river's rise with the weather drawn (off: at the level it was generated at)
};

/*
	The water as the debugger's World tab shows it (shell/host/inspectors.h):
	how much of it's running and how far it's stirred, and switches to see
	inside it (what's drawn, by what, and as what), stop it, or poke it.
*/
class Inspect
{
public:
	struct Opts
	{
		Grid &grid;
		Look &look;
		const Base &base;
		const Flat &flat;
	};
	explicit Inspect(Opts o);

	// A frame: a river's rise where the camera is (sixteenths), as the weather has it
	void frame(float rise_here) { m_rise_here = rise_here; }
	// Its stats and controls now (InspectorState, bridge/types.ts)
	Json::Value describe();
	// A control used: why not, for one it hasn't or a value it can't take, or nothing
	std::optional<std::string> act(const std::string &control, const Json::Value &value);

private:
	std::optional<std::string> splash();
	void calm();

	Grid &m_grid;
	Look &m_look;
	const Base &m_base;
	const Flat &m_flat;
	u64 m_retargeted = 0; // tiles' targets found again, as of the last description
	s64 m_at = 0;         // when that was (µs)
	float m_rate = 0;     // ... per s since
	float m_rise_here = 0;
};

} // namespace liquids
