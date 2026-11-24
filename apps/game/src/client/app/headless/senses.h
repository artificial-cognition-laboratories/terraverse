// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irr_v3d.h"
#include <json/json.h>
#include <vector>

class Client;

namespace headless {

/*
	What a body senses (axon.body/1's senses), read from its player after each step. SI and the
	contract's frames: the place `world` is local (x east, y north, z up, a node a metre); the
	body's base and head are flu (x forward, y left, z up).

	  /imu          head    the specific force and turn an IMU in the head would read
	  /contact      base    on the ground, climbing, in a liquid
	  /interoception        health and breath, as fractions; dead
	  /position     world   where it is and which way it looks: the labelled tier
	  /blocks       world   the blocks round it, named: the labelled tier
	  /chat                 a line someone said, and who
	  /joints               every joint of the figure, as it's posed (rig.h)

	A sense says when it's due at its own rate; the rest of a step's readings wait.
*/
class Senses
{
public:
	struct Opts
	{
		Client &client;
	};
	explicit Senses(Opts o);

	// The senses as the manifest declares them.
	static Json::Value declared();

	// Readings due at `now` (ms, the body's clock), the player having just moved `dtime` (s),
	// looking `yaw` and `pitch` (degrees, as the engine keeps them). Each one {ch, at, v} or {ch, at, text}.
	std::vector<Json::Value> read(float dtime, unsigned long long now, float yaw, float pitch);

private:
	struct Due
	{
		unsigned long long every; // ms
		unsigned long long next = 0;
	};
	bool due(Due &d, unsigned long long now);

	Client &m_client;
	v3f m_last_speed;  // m/s, the world's axes
	float m_last_yaw = 0, m_last_pitch = 0;
	bool m_moved = false;
	Due m_imu{ 50 }, m_contact{ 50 }, m_position{ 50 }, m_interoception{ 250 }, m_blocks{ 500 }, m_joints{ 50 };
};

} // namespace headless
