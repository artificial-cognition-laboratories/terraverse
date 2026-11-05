// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "driven.h"
#include <cmath>

namespace physics {

Driven::Driven(Actor::Boxes boxes, std::function<float(const v3d &)> surface, const Assembly &assembly, const v3d &at, std::vector<Cue> cues) :
	m_boxes(std::move(boxes)), m_cues(std::move(cues))
{
	World::Opts opts;
	opts.field = [](const v3d &, const v3f &) { return Here{ v3f(0, -9.81f, 0), 1.2f }; };
	opts.ground = [this](v3s16 block, std::vector<aabb3f> &out) {
		std::vector<aabb3f> node;
		for (s16 x = 0; x < BLOCK; x++)
			for (s16 y = 0; y < BLOCK; y++)
				for (s16 z = 0; z < BLOCK; z++) {
					node.clear();
					m_boxes(v3s16(block.X * BLOCK + x, block.Y * BLOCK + y, block.Z * BLOCK + z), node);
					const v3f by(x, y, z);
					for (aabb3f b : node) {
						b.MinEdge += by;
						b.MaxEdge += by;
						out.push_back(b);
					}
				}
		return true;
	};
	opts.surface = std::move(surface);
	m_world = std::make_unique<World>(opts);
	m_id = m_world->add(assembly, Pose{ at, Quat{} });
	m_world->drive(m_id, Drive{});
	m_was_pose = m_world->pose(m_id);
	m_was_relatives = m_world->relatives(m_id);
}

Driven::~Driven() = default;

float Driven::pace() const
{
	const v3f v = m_world->velocity(m_id);
	return std::hypot(v.X, v.Z);
}

static Quat between(const Quat &a, const Quat &b, float t)
{
	const float side = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w < 0 ? -t : t; // the short way round
	const Quat q{ a.x * (1 - t) + b.x * side, a.y * (1 - t) + b.y * side, a.z * (1 - t) + b.z * side, a.w * (1 - t) + b.w * side };
	const float size = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
	return Quat{ q.x / size, q.y / size, q.z / size, q.w / size };
}

Pose Driven::drawnPose() const
{
	const Pose now = m_world->pose(m_id);
	const double t = m_behind / STEP;
	return Pose{ m_was_pose.position + (now.position - m_was_pose.position) * t, between(m_was_pose.rotation, now.rotation, (float)t) };
}

std::vector<Relative> Driven::drawnRelatives() const
{
	std::vector<Relative> out = m_world->relatives(m_id);
	const float t = (float)(m_behind / STEP);
	for (size_t i = 0; i < out.size() && i < m_was_relatives.size(); i++) {
		out[i].offset = m_was_relatives[i].offset + (out[i].offset - m_was_relatives[i].offset) * t;
		out[i].rotation = between(m_was_relatives[i].rotation, out[i].rotation, t);
	}
	return out;
}

void Driven::update(float dt)
{
	m_behind += dt;
	while (m_behind >= STEP) {
		m_time += STEP;
		m_behind -= STEP;
		while (m_cue < m_cues.size() && m_cues[m_cue].at <= m_time) {
			const Cue &next = m_cues[m_cue++];
			m_world->drive(m_id, next.drive);
			if (!next.say.empty())
				m_doing = next.say;
		}
		// someone well above it, so it's kept Live and never met
		m_world->players({ Capsule{ pose().position + v3d(0, 30, 0), 0.3f, 1.77f } });
		m_was_pose = m_world->pose(m_id);
		m_was_relatives = m_world->relatives(m_id);
		m_world->step(STEP);
	}
}

} // namespace physics
