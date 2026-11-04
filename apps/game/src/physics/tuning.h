// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

namespace physics {

// The movement numbers being tuned by feel, live from the world controls. Only
// the player's own client reads them; settings `tune_<name>` hold them (the
// menu's ranges are in shell/options.cpp, its sliders in tf_player/controls.lua).
struct Tuning
{
	float kick_out = 4.5f;        // nodes/s a wall kick drives out from the wall
	float kick_steer = 3.5f;      // nodes/s along where it's looking
	float wall_run_time = 1.2f;   // s at most
	float wall_run_lift = 2;      // nodes/s up, as it takes to the wall
	float wall_run_gravity = 0.35f; // × the pull
	float shimmy = 1.2f;          // nodes/s
	float hard_landing = 7;       // nodes/s down: a roll takes it, or it costs speed
	float roll_late = 0.2f;       // s after a hard landing a crouch still makes it a roll
	float falls_from = 0.85f;     // of landingStagger
	float falls_at = 11;          // nodes/s down, however it landed
};

inline Tuning &tuning()
{
	static Tuning t;
	return t;
}

struct TuningKnob
{
	const char *setting;
	float Tuning::*field;
};

inline constexpr TuningKnob TUNING_KNOBS[] = {
	{ "tune_kick_out", &Tuning::kick_out },
	{ "tune_kick_steer", &Tuning::kick_steer },
	{ "tune_wall_run_time", &Tuning::wall_run_time },
	{ "tune_wall_run_lift", &Tuning::wall_run_lift },
	{ "tune_wall_run_gravity", &Tuning::wall_run_gravity },
	{ "tune_shimmy", &Tuning::shimmy },
	{ "tune_hard_landing", &Tuning::hard_landing },
	{ "tune_roll_late", &Tuning::roll_late },
	{ "tune_falls_from", &Tuning::falls_from },
	{ "tune_falls_at", &Tuning::falls_at },
};

} // namespace physics
