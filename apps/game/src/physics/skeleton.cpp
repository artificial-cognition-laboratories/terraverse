// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "physics/skeleton.h"


#include <algorithm>
#include <cmath>

namespace physics {

namespace {

constexpr float DEG = 180.f / 3.14159265f;

core::matrix4 axes(v3f x, v3f y, v3f z)
{
	core::matrix4 m;
	m[0] = x.X; m[1] = x.Y; m[2] = x.Z;
	m[4] = y.X; m[5] = y.Y; m[6] = y.Z;
	m[8] = z.X; m[9] = z.Y; m[10] = z.Z;
	return m;
}

v3f turnedAbout(v3f v, v3f axis, float angle)
{
	const float c = std::cos(angle), s = std::sin(angle);
	return v * c + axis.crossProduct(v) * s + axis * (axis.dotProduct(v) * (1 - c));
}

// An arm through its own chest or hips, or into what's beside it, swung away at its shoulder till it's clear:
// what the body's parts did of themselves when they were bodies (sprint 105). Most often a key pose's arms
// passing through on their way to another. A hand that's on what it holds is where it's meant to be.
void keepArmsOut(Bones &b, const std::function<v3f(v3f, float)> &clear, const Grip *grip, const float (&on)[2])
{
	constexpr float S = FIGURE_SCALE, STEP = 0.03f; // radians at a time: no further than it must
	constexpr int MOST = 40;
	const float r = BONE_NARROW / 2 * S;
	const core::matrix4 into_hips = b.chest.matrix(), chest = b.lower.matrix() * b.hips.matrix() * b.chest.matrix();
	core::matrix4 from_figure;
	chest.getInverse(from_figure);
	const v3f chest_mid(0, CHEST_TALL / 2 * S, 0), chest_half = v3f(0.5f, CHEST_TALL + 0.02f, BONE_WIDE) * (S / 2);
	const v3f hips_mid(0, HIPS_TALL / 2 * S, 0), hips_half = v3f(0.48f, HIPS_TALL, 0.23f) * (S / 2);
	const auto depth = [&](v3f p, v3f mid, v3f half) {
		const v3f d = p - mid;
		return std::min({ half.X + r - std::fabs(d.X), half.Y + r - std::fabs(d.Y), half.Z + r - std::fabs(d.Z) });
	};
	for (int side : { 0, 1 }) {
		if (grip && grip->held[side] && on[side] > 0.95f) // (still on its way there, it goes round what's in the way)
			continue;
		bool was_world = false; // the last turn was out of the world
		v3f before;
		for (int i = 0; i < MOST; i++) {
			// the elbow, the forearm's middle, the wrist and the fingers' ends, in the chest's frame
			const core::matrix4 upper = b.upper[side].matrix(), fore = upper * b.fore[side].matrix();
			v3f points[4] = { v3f(0), v3f(0, -FOREARM / 2 * S, 0), v3f(0, -FOREARM * S, 0), v3f(0, -HAND_LONG * S, 0) };
			(fore * b.hand[side].matrix()).transformVect(points[3]);
			float deepest = 0, in_body = 0, in_world = 0;
			v3f worst, away, body_worst, body_away, world_worst, world_away;
			for (int k = 0; k < 4; k++) {
				v3f &p = points[k];
				if (k < 3)
					fore.transformVect(p);
				v3f in_hips = p, in_figure = p;
				into_hips.transformVect(in_hips);
				chest.transformVect(in_figure);
				// out of itself the shortest way, away from its middle; out of the world as the world has it
				const float body = std::max(depth(p, chest_mid, chest_half), depth(in_hips, hips_mid, hips_half));
				const v3f world = from_figure.rotateAndScaleVect(clear(in_figure, r));
				if (body > in_body) {
					in_body = body;
					body_worst = p;
					body_away = v3f(p.X, 0, p.Z).getLengthSQ() > 1e-6f ? v3f(p.X, 0, p.Z) : v3f(side ? -1.f : 1.f, 0, 0);
				}
				if (world.getLength() > in_world) {
					in_world = world.getLength();
					world_worst = p;
					world_away = world;
				}
			}
			// out of itself first, always; out of what's beside it only so far as that keeps it out of itself
			if (in_body > 0 && was_world) {
				b.upper[side].turn = before;
				break;
			}
			was_world = in_body <= 0;
			deepest = in_body > 0 ? in_body : in_world;
			worst = in_body > 0 ? body_worst : world_worst;
			away = in_body > 0 ? body_away : world_away;
			if (deepest <= 0)
				break;
			before = b.upper[side].turn;
			v3f axis = (worst - b.upper[side].at).crossProduct(away.normalize());
			if (axis.getLengthSQ() < 1e-6f)
				break; // (straight out that way already: turning won't help)
			axis.normalize();
			const auto turned = [&](v3f v) { return turnedAbout(upper.rotateAndScaleVect(v), axis, STEP); };
			b.upper[side].turn = axes(turned(v3f(1, 0, 0)), turned(v3f(0, 1, 0)), turned(v3f(0, 0, 1))).getRotationDegrees();
		}
	}
}

} // namespace

core::matrix4 BonePlace::matrix() const
{
	core::matrix4 m;
	m.setRotationDegrees(turn);
	m.setTranslation(at);
	return m;
}

Bones Skeleton::place(float dt, const Joints &j, float tilt, const Grip *grip, v3f clear)
{
	const float S = FIGURE_SCALE;
	Bones b;
	b.hips.at = v3f(0, (HIP_HEIGHT - j.drop) * S, 0);
	// Turned from the view, over in a roll, and into its shoulder first (read right to left).
	core::matrix4 twist, spin, lean, tipped;
	tipped.setRotationRadians(v3f(0, 0, tilt));
	twist.setRotationRadians(v3f(0, j.twist, 0));
	spin.setRotationRadians(v3f(j.spin, 0, 0));
	lean.setRotationRadians(v3f(0, j.spin_lean, 0));
	const core::matrix4 turn = tipped * twist * spin * lean;
	b.lower.turn = turn.getRotationDegrees();
	// a roll turns about the body's middle, not its feet
	const v3f middle(0, 0.45f, 0);
	const v3f moved = turn.rotateAndScaleVect(middle);
	const v3f backed = twist.rotateAndScaleVect(v3f(0, 0, -j.back));
	b.lower.at = (middle - moved + backed) * S;
	// hung, all of it goes round its hands, in the frame of the hips: hands stay, the rest swings under them
	if (j.swing_back != 0 || j.swing_side != 0) {
		if (grip && (grip->held[0] || grip->held[1]))
			m_pivot = grip->held[0] && grip->held[1] ? (grip->wrist[0] + grip->wrist[1]) / 2 : grip->wrist[grip->held[0] ? 0 : 1];
		const v3f pivot = m_pivot;
		core::matrix4 turned, unturn, swung, to, from;
		twist.getInverse(unturn);
		swung.setRotationRadians(v3f(j.swing_back, 0, j.swing_side));
		to.setTranslation(pivot);
		from.setTranslation(pivot * -1.f);
		turned = to * twist * swung * unturn * from * b.lower.matrix();
		b.lower.at = turned.getTranslation();
		b.lower.turn = turned.getRotationDegrees();
	}
	// over about its feet, in the frame of the hips; a little up off the ground, it's a body lying on it
	if (j.fallen != 0) {
		core::matrix4 unturn, over, raised;
		twist.getInverse(unturn);
		over.setRotationRadians(v3f(j.fallen, 0, 0));
		raised.setTranslation(v3f(0, 0.12f * std::fabs(j.fallen) / 1.5f, 0));
		// (it's moved along the ground after it's turned over: turned with it, that move would lift it off)
		core::matrix4 unmoved = b.lower.matrix();
		unmoved.setTranslation(v3f(0));
		const core::matrix4 turned = raised * twist * over * unturn * unmoved;
		b.lower.at += turned.getTranslation();
		b.lower.turn = turned.getRotationDegrees();
	}
	// (the lean's in the hips: the legs stay under it, on the feet)
	b.hips.turn = v3f((j.pitch + j.waist * 0.3f + j.lean_ahead) * DEG, 0, -j.lean_right * DEG);
	b.chest.at = v3f(0, HIPS_TALL * S, 0);
	// the chest stays mostly with the hips; the head does the turning
	b.chest.turn = v3f(j.waist * 0.7f * DEG, j.chest_twist * DEG, 0);
	(void)clear; // (kept off what it's beside by leaning: Posing, once what it strides or plays has its chest)
	// The head's own turn is what's left between its parents and the view. Summing angles
	// isn't enough: on a bent body they don't add, and it rolled over sideways.
	const core::matrix4 chest = b.lower.matrix() * b.hips.matrix() * b.chest.matrix();
	core::matrix4 parents = chest;
	parents.setTranslation(v3f(0));
	core::matrix4 undo, view;
	parents.getInverse(undo);
	view.setRotationDegrees(v3f(j.head * DEG, 0, 0));
	b.head.at = v3f(0, CHEST_TALL * S, 0);
	// (rolling, it tucks and goes round with the body)
	b.head.turn = j.spin != 0 || std::fabs(j.fallen) > 0.3f ? v3f(0.5f * DEG, 0, 0) : (undo * view).getRotationDegrees();
	for (int side : { 0, 1 }) {
		const float x = side ? -1.f : 1.f;
		b.thigh[side] = { v3f(HIP_WIDE * x, HIP_HEIGHT - j.drop, 0) * S, v3f(j.hip[side] * DEG, 0, j.hip_out[side] * DEG) };
		if (j.crawl[side] >= 0) {
			// Crawling, the leg's laid out along the floor behind: the thigh back and, drawn up, out to the side, the
			// shin back in under it. Both just off the floor; the knee bends in the plane they make, as a knee does.
			constexpr float OFF_FLOOR = 0.08f;
			const float up = j.crawl[side];
			const v3f hip = b.thigh[side].at;
			const float out = 1.5f * up;
			const float t_len = THIGH * S, s_len = physics::SHIN * S;
			v3f d(x * std::sin(out), 0, -std::cos(out));
			d.Y = std::clamp((OFF_FLOOR - hip.Y) / t_len, -0.9f, 0.f);
			d.normalize();
			const v3f knee = hip + d * t_len;
			v3f sd(-x * 0.2f * up, 0, -1);
			sd.Y = std::clamp((OFF_FLOOR - knee.Y) / s_len, -0.9f, 0.9f);
			sd.normalize();
			v3f across = d.crossProduct(sd);
			if (across.getLengthSQ() < 1e-4f)
				across = v3f(1, 0, 0);
			across.normalize();
			if (across.crossProduct(d * -1.f).Y > 0) // (the front of the thigh to the floor, as it lies)
				across *= -1.f;
			const core::matrix4 thigh = axes(across, d * -1.f, across.crossProduct(d * -1.f));
			const core::matrix4 shin = axes(across, sd * -1.f, across.crossProduct(sd * -1.f));
			core::matrix4 undo_thigh;
			thigh.getInverse(undo_thigh);
			b.thigh[side].turn = thigh.getRotationDegrees();
			b.shin[side].turn = (undo_thigh * shin).getRotationDegrees();
		}

		b.shin[side] = { v3f(0, -THIGH * S, 0), v3f(j.knee[side] * DEG, 0, 0) };
		b.upper[side] = { v3f(SHOULDER_OUT * x, SHOULDER_UP, 0) * S, v3f(j.shoulder[side] * DEG, 0, j.shoulder_out[side] * DEG) };
		b.fore[side] = { v3f(0, -UPPER_ARM * S, 0), v3f(j.elbow[side] * DEG, 0, 0) };
		b.hand[side] = { v3f(0, -FOREARM * S, 0), v3f(0) };
	}
	// Hands on what they've hold of: from where the pose has them, so they reach for it and come away
	core::matrix4 from_chest;
	chest.getInverse(from_chest);
	for (int side : { 0, 1 }) {
		constexpr float REACHES = 16; // about a tenth of a second there
		const bool holds = grip && grip->held[side];
		if (holds) {
			m_grip_at[side] = grip->wrist[side];
			m_grip_over = grip->over;
		}
		m_gripping[side] += ((holds ? 1.f : 0.f) - m_gripping[side]) * std::min(1.f, REACHES * dt);
		const float w = m_gripping[side];
		if (w < 0.01f) {
			m_wrist_known[side] = false;
			continue;
		}
		const float x = side ? -1.f : 1.f;
		const v3f shoulder = b.upper[side].at;
		const core::matrix4 posed = b.upper[side].matrix() * b.fore[side].matrix();
		v3f free(0, -FOREARM * S, 0);
		posed.transformVect(free);
		// a hand's on top of what it holds: its wrist a forearm's thickness over (sprint 105: as the body did)
		v3f wanted = m_grip_at[side] + v3f(0, BONE_NARROW / 2 * S, 0);
		// on its way, it comes out from and over the top of what it's taking, not up through its face
		constexpr float OVER = 0.09f; // nodes, halfway there
		wanted.Y += OVER * 4 * (1 - w); // (the wrist goes w of the way: OVER at most, at half)
		if (m_grip_over.Y < 0.5f)
			wanted -= v3f(m_grip_over.X, 0, m_grip_over.Z) * (OVER * 4 * (1 - w));
		from_chest.transformVect(wanted);
		v3f wrist = free + (wanted - free) * w;
		// planted, it stays planted however the body goes; going to or from, it's an arm with a top speed
		constexpr float MOST_HAND_SPEED = 14; // nodes/s: a quick arm's (sprint 105: at 9, caught at a run, it lagged under the edge)
		if (m_wrist_known[side] && !(holds && w > 0.9f)) {
			const v3f move = wrist - m_wrist[side];
			const float most = MOST_HAND_SPEED * dt;
			if (move.getLength() > most)
				wrist = m_wrist[side] + move / move.getLength() * most;
		}
		m_wrist[side] = wrist;
		m_wrist_known[side] = true;
		const ArmTo arm = armTo((wrist - shoulder) / S, v3f(0.7f * x, -0.6f, -0.7f), UPPER_ARM, FOREARM);
		const core::matrix4 upper = axes(arm.x, arm.y, arm.z);
		b.upper[side].turn = upper.getRotationDegrees();
		b.fore[side].turn = v3f(arm.elbow * DEG, 0, 0);
		// the hand turned over onto it
		core::matrix4 elbow;
		elbow.setRotationDegrees(b.fore[side].turn);
		const core::matrix4 fore = upper * elbow;
		const v3f down = fore.rotateAndScaleVect(v3f(0, -1, 0));
		const v3f over = from_chest.rotateAndScaleVect(m_grip_over);
		// (hooked over it, not laid flat on it: a wrist only bends so far)
		constexpr float HOOKS = 0.55f;
		v3f finger = down + (over - down) * (w * HOOKS);
		// pushing down on top of what it holds, the hand's flat on it: its fingers don't go into it (sprint 105)
		if (m_grip_over.Y < 0.5f) {
			const v3f top = from_chest.rotateAndScaleVect(v3f(0, 1, 0));
			if (const float into = finger.dotProduct(top); into < 0)
				finger -= top * into;
		}
		finger.normalize();
		const v3f y = finger * -1.f;
		v3f across = fore.rotateAndScaleVect(v3f(1, 0, 0));
		across -= y * across.dotProduct(y);
		across.normalize();
		core::matrix4 undo_fore;
		fore.getInverse(undo_fore);
		b.hand[side].turn = (undo_fore * axes(across, y, across.crossProduct(y))).getRotationDegrees();
	}
	return b;
}

Bones Posing::pose(float dtime, v3f velocity, float pitch, bool airborne, Stance stance, float turned, std::optional<float> face,
		float tilt, const Contacts &contacts, const Ground &ground, const Clear &clear)
{
	PoseIn in;
	in.moving = velocity;
	in.pitch = pitch;
	const bool climbing = stance == Stance::VAULT || stance == Stance::MANTLE || stance == Stance::HANG || stance == Stance::WALL_RUN || stance == Stance::DOWN;
	in.airborne = airborne && !climbing;
	in.stance = stance;
	in.push = contacts.push;
	const Grip *grip = contacts.grip;
	if (grip)
		in.hand_moving = grip->moving;
	if (contacts.step_on) {
		in.steps = true;
		in.step_on = *contacts.step_on / FIGURE_SCALE;
	}
	in.feet = contacts.feet;
	in.fallen = contacts.fallen;
	in.rising = contacts.rising;
	in.stagger = contacts.stagger;
	in.throw_side = contacts.throw_side;
	in.throw_back = contacts.throw_back;
	if (airborne) // (the ground under it: where it comes down)
		in.above = std::max(0.f, -ground(v3f(0)));
	in.turned = turned;
	if (face) {
		in.faces = true;
		in.face = *face;
	}
	for (int side : { 0, 1 }) {
		// asked in the figure's scale, as its feet are drawn; no point looking from the air
		const v3f foot = m_poser.footAt(side) * FIGURE_SCALE;
		in.ground[side] = in.airborne || climbing ? 0 : ground(foot) / FIGURE_SCALE;
	}
	const Joints j = m_poser.step(dtime, in);

	Bones b = m_skeleton.place(dtime, j, tilt, grip, m_clear);
	// On its feet and going, stood or crouched, it strides as a person does: the recording's, at its own pace.
	{
		// (in the air, only from a run: a leap, a stride of it; Strides says)
		const bool on = (stance == Stance::STAND || stance == Stance::CROUCH) && !face && !(grip && (grip->held[0] || grip->held[1])) &&
				contacts.stagger < 0.3f &&
				contacts.push.getLengthSQ() < 0.01f;
		// (off its feet, rolling, sliding, down, climbing, it lets go at once: what that is is the stance's, and the
		// change is eased from below)
		if (stance != Stance::STAND && stance != Stance::CROUCH)
			m_strides.drop();
		b = m_strides.step(dtime, b, velocity, turned, on, stance == Stance::CROUCH, airborne, velocity.Y,
				airborne ? std::max(0.f, -ground(v3f(0))) : 0.f);
		for (int side : { 0, 1 })
			m_planted[side] = !airborne && (m_strides.share() > 0.5f ? m_strides.planted(side) : j.planted[side]);
	}
	// A jump's a person's jump, as recorded (Moves): off the ground on its feet, not off a wall, and not once
	// there's something its hands are going for: that's a climb's
	m_let_go = false;
	{
		const bool jumps = stance == Stance::STAND && !face && !grip && contacts.push.getLengthSQ() < 0.01f;
		const bool striking = m_moves.striking() && (stance == Stance::STAND || stance == Stance::CROUCH) && !face &&
				!(grip && (grip->held[0] || grip->held[1]));
		if (!jumps && !striking)
			m_let_go = m_moves.stop();
		else if (airborne && !m_was_airborne && velocity.Y > 1 && !m_strides.leaping() && !m_moves.playing()) // (from a run it leaps: Strides)
			m_moves.jump();
		m_was_airborne = airborne;
		// a punch or a kick, on its feet or in the air (not hanging off something, or lying down)
		const bool can_strike = (stance == Stance::STAND || stance == Stance::CROUCH) && !face && !(grip && (grip->held[0] || grip->held[1]));
		if (contacts.strike == 2 && can_strike && !airborne && std::hypot(velocity.X, velocity.Z) >= FLYING_KICK_FROM)
			m_moves.flyingKick(); // (the mover hops it)
		else if (contacts.strike && can_strike)
			m_moves.strike(contacts.strike == 2 ? Strike::KICK : Strike::PUNCH);
		b = m_moves.step(dtime, b, airborne, velocity.Y, airborne ? std::max(0.f, -ground(v3f(0))) : 0.f,
				airborne || m_strides.share() > 0.3f);
		m_hit = m_moves.takeHit();
		m_let_go = m_let_go || m_moves.followedOn();
	}
	// It looks where it's looking, as it does stood: the recordings' heads were looking at their own ground. A
	// little of how they went is kept, so it isn't a camera on a stick (sprint 105)
	{
		// (upright only: a rolling head's the roll's, and turned to a view it'd go the long way round)
		const bool footed = stance == Stance::STAND || stance == Stance::CROUCH;
		const float recorded = std::max(m_strides.share(), m_moves.share());
		if (footed && recorded > 0) {
			constexpr float LOOKS = 0.85f;
			core::matrix4 parents = b.lower.matrix() * b.hips.matrix() * b.chest.matrix(), undo, view;
			parents.setTranslation(v3f(0));
			parents.getInverse(undo);
			view.setRotationDegrees(v3f(j.head * DEG, 0, 0));
			// as far round as a neck turns, and no further: off a wall the body may be well round from where it looks
			constexpr float NECK = 1.4f; // radians
			const core::matrix4 wanted = undo * view;
			const v3f ahead = wanted.rotateAndScaleVect(v3f(0, 0, 1));
			const float round = std::acos(std::clamp(ahead.Z, -1.f, 1.f));
			Bones square = b, looking = b;
			square.head.turn = v3f(0);
			looking.head.turn = wanted.getRotationDegrees();
			if (round > NECK)
				looking = blend(square, looking, NECK / round);
			b = blend(b, looking, LOOKS * recorded);
		}
	}
	// Down, it's the shape it holds that's asked: legs out, a little bent, not under it.
	if (stance == Stance::DOWN && contacts.rising <= 0)
		for (int side : { 0, 1 }) {
			b.thigh[side].turn = v3f(-20, 0, side == 0 ? 8.f : -8.f);
			b.shin[side].turn = v3f(30, 0, 0);
		}
	// On the ground whole, the lowest of it rests on it: it's lifted till nothing's asked to be in the floor.
	if (stance == Stance::ROLL || stance == Stance::DOWN || stance == Stance::PRONE || stance == Stance::SLIDE) {
		constexpr float RESTS = 0.02f, FLOOR_NOT_WALL = 0.3f;
		float under = 0;
		eachBoneBox(b, [&](const core::matrix4 &m, v3f size, v3f centre, BoneColour) {
			for (const float x : { -0.5f, 0.5f })
				for (const float y : { -0.5f, 0.5f })
					for (const float z : { -0.5f, 0.5f }) {
						const v3f at = m.transformVect(centre + v3f(size.X * x, size.Y * y, size.Z * z));
						const float floor = ground(at);
						if (floor <= FLOOR_NOT_WALL)
							under = std::max(under, floor + RESTS - at.Y);
					}
		});
		b.lower.at.Y += under;
	}
	// Its head and shoulders kept off what's beside them by leaning away from it at the waist, as far as a back
	// bends, not slid off its hips (sprint 105: by a wall its torso came away from its legs)
	if (m_clear.getLengthSQ() > 1e-8f) {
		constexpr float MOST_LEAN = 0.45f; // radians
		core::matrix4 under = b.lower.matrix() * b.hips.matrix(), undo_under;
		under.setTranslation(v3f(0));
		under.getInverse(undo_under);
		v3f away = undo_under.rotateAndScaleVect(m_clear); // in the hips' frame
		away.Y = 0;
		const float far = away.getLength();
		if (far > 1e-4f) {
			const float lever = (CHEST_TALL + HEAD_SIZE / 2) * FIGURE_SCALE; // (from the waist to the middle of its head)
			const float angle = std::min(std::atan2(far, lever), MOST_LEAN);
			const v3f axis = v3f(0, 1, 0).crossProduct(away / far);
			const core::matrix4 chest = b.chest.matrix();
			const auto leaned = [&](v3f v) { return turnedAbout(chest.rotateAndScaleVect(v), axis, angle); };
			b.chest.turn = axes(leaned(v3f(1, 0, 0)), leaned(v3f(0, 1, 0)), leaned(v3f(0, 0, 1))).getRotationDegrees();
		}
	}
	// A new stance, striding the other way at once, a move let go of for a climb, or one strike into the next,
	// eases in from where the last left it, rather than snapping to its own (sprint 105). Before its head's kept
	// out of what it's beside: what it's eased from may have been somewhere else
	constexpr float CHANGES = 0.15f; // s
	if (m_posed && (stance != m_stance || m_strides.turnedRound() || m_let_go)) {
		m_from = m_last;
		m_since = 0;
	}
	m_stance = stance;
	m_since += dtime;
	if (m_posed && m_since < CHANGES) {
		const float t = m_since / CHANGES;
		b = blend(m_from, b, t * t * (3 - 2 * t));
	}
	// Its head and shoulders go round what they meet. Asked where they'd be without the last
	// answer, so being moved clear doesn't read as there being nothing to clear.
	const bool upright = stance != Stance::ROLL && stance != Stance::PRONE && stance != Stance::SLIDE && stance != Stance::DOWN;
	v3f wanted;
	float head_in = 0; // how far its head's into something
	if (upright) {
		const core::matrix4 chest = b.lower.matrix() * b.hips.matrix() * b.chest.matrix();
		const auto ball = [&](const core::matrix4 &m, v3f at, float r) {
			m.transformVect(at);
			const v3f by = clear(at - m_clear, r);
			if (by.getLengthSQ() > wanted.getLengthSQ())
				wanted = by;
			return by.getLength();
		};
		head_in = ball(chest * b.head.matrix(), v3f(0, HEAD_SIZE / 2, 0) * FIGURE_SCALE, 0.27f);
		for (int side : { 0, 1 })
			ball(chest, b.upper[side].at, 0.14f);
	}
	if (wanted.getLength() > 0.3f)
		wanted = wanted / wanted.getLength() * 0.3f;
	// Over a stride or so, not each swing of its shoulders: beside a wall it keeps a little off it, and doesn't
	// bob away each step (sprint 105: a sprint's shoulders swing wide, and its head shook with them). But a head
	// that's gone into something comes out of it at once (only its head: a shoulder brushing past isn't that)
	constexpr float KEEPS_OFF = 5, GETS_OUT = 18, DEEP = 0.08f; // /s, /s, nodes
	const float rate = head_in > DEEP ? GETS_OUT : KEEPS_OFF;
	m_clear += (wanted - m_clear) * std::min(1.f, rate * dtime);
	const float on[2] = { m_skeleton.gripping(0), m_skeleton.gripping(1) };
	keepArmsOut(b, clear, grip, on);
	m_last = b;
	m_posed = true;
	return b;
}

void eachBoneBox(const Bones &b, const std::function<void(const core::matrix4 &, v3f, v3f, BoneColour)> &box)
{
	constexpr float S = FIGURE_SCALE;
	const auto hanging = [&](const core::matrix4 &m, float length, float wide, BoneColour c) {
		box(m, v3f(wide, length + BONE_OVERLAP, wide) * S, v3f(0, (BONE_OVERLAP - length) / 2, 0) * S, c);
	};
	const core::matrix4 lower = b.lower.matrix(), hips = lower * b.hips.matrix(), chest = hips * b.chest.matrix();
	box(hips, v3f(0.48f, HIPS_TALL, 0.23f) * S, v3f(0, HIPS_TALL / 2, 0) * S, BoneColour::TROUSERS);
	box(chest, v3f(0.5f, CHEST_TALL + 0.02f, BONE_WIDE) * S, v3f(0, CHEST_TALL / 2, 0) * S, BoneColour::SHIRT);
	box(chest * b.head.matrix(), v3f(HEAD_SIZE) * S, v3f(0, HEAD_SIZE / 2, 0) * S, BoneColour::SKIN);
	for (int side : { 0, 1 }) {
		const core::matrix4 thigh = lower * b.thigh[side].matrix(), upper = chest * b.upper[side].matrix(), fore = upper * b.fore[side].matrix();
		hanging(thigh, THIGH, BONE_WIDE, BoneColour::TROUSERS);
		hanging(thigh * b.shin[side].matrix(), SHIN, BONE_NARROW, BoneColour::TROUSERS);
		hanging(upper, UPPER_ARM, BONE_WIDE, BoneColour::SHIRT);
		hanging(fore, FOREARM, BONE_NARROW, BoneColour::SHIRT);
		box(fore * b.hand[side].matrix(), v3f(BONE_WIDE, HAND_LONG, BONE_WIDE) * S, v3f(0, -HAND_LONG / 2, 0) * S, BoneColour::SKIN);
	}
}

} // namespace physics
