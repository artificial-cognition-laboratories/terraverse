// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/game/objects/player.h"
#include <json/json.h>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace headless {

/*
	What a body can be told to do, and the doing of it (axon.body/1's controls): `/legs` and
	`/hands`, switches; `/look`, the view's yaw and pitch as velocities in rad/s. The same
	channels as the Minecraft body's, so a mind that wore one wears this.

	A command holds for HOLD ms and is then let go (the control's lapse): a mind that goes quiet
	leaves a body that stops, never one walking off a cliff on its last command. What's out of
	range is clamped, and `applied` says what took effect. It reaches the player as a
	PlayerControl, as a person's keys and mouse do (Game::updatePlayerControl).
*/
class Motor
{
public:
	// How long a command holds before it lapses, ms.
	static constexpr unsigned HOLD = 250;
	// The fastest a view turns, rad/s.
	static constexpr float TURN = 6.2832f;

	// What took effect of one command, after clamping: the body contract's `applied`.
	struct Applied
	{
		std::string channel;
		unsigned long long seq;
		std::vector<float> values;
	};
	// One step's doing: what the player's told, and the commands that took effect in it.
	struct Step
	{
		PlayerControl control;
		std::vector<Applied> applied;
	};

	// The controls as the manifest declares them.
	static Json::Value declared();

	// A command on one channel, at `now` (ms, the body's clock). Why not, when it's refused.
	std::optional<std::string> accept(const std::string &channel, unsigned long long seq,
			const std::vector<float> &values, unsigned long long now);
	// What the player does for `dtime` (s), at `now`: held commands applied, lapsed ones let go.
	Step step(float dtime, unsigned long long now);
	// The view, in degrees as the engine keeps it: yaw + left, pitch + down.
	float yaw() const { return m_yaw; }
	float pitch() const { return m_pitch; }
	// Turned by the world, not the mind: a teleport, crossing a planet's seam.
	void face(float yaw, float pitch);
	void turn(float yaw) { m_yaw += yaw; }

private:
	struct Held
	{
		std::vector<float> values;
		unsigned long long seq;
		unsigned long long until;
		bool answered;
	};
	std::map<std::string, Held> m_held;
	float m_yaw = 0, m_pitch = 0;
};

} // namespace headless
