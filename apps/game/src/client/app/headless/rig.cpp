// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/headless/rig.h"
#include <string>

namespace headless {

namespace rig {

namespace {

constexpr float RAD = 0.01745329252f;

Json::Value vec(v3f v)
{
	Json::Value a(Json::arrayValue);
	a.append(v.X);
	a.append(v.Y);
	a.append(v.Z);
	return a;
}

Json::Value quaternion(float x, float y, float z, float w)
{
	Json::Value q(Json::arrayValue);
	for (float c : { x, y, z, w })
		q.append(c);
	return q;
}

Json::Value frame(const std::string &name, const char *parent, const char *axes, v3f at, Json::Value turn)
{
	Json::Value f;
	f["name"] = name;
	f["parent"] = parent ? Json::Value(parent) : Json::Value();
	f["axes"] = axes;
	f["at"] = vec(at);
	f["turn"] = turn;
	return f;
}

/*
	The three joints a bone hangs by, outermost first, and the engine axis each turns about, as
	the contract has it. Irrlicht's turn is Rz·Ry·Rx in its own numbers; moved into flu by P
	(x→−y, y→z, z→x, a mirror: det −1), a turn θ about an axis a is one θ about −P·a.
*/
struct Joint
{
	const char *suffix;
	v3f axis; // flu
};
const Joint JOINTS[] = {
	{ ".z", v3f(-1, 0, 0) }, // the engine's z (ahead) is flu's x
	{ ".y", v3f(0, 0, -1) }, // its y (up), flu's z
	{ "", v3f(0, 1, 0) },    // its x (right), flu's −y
};

} // namespace

v3f flu(v3f engine)
{
	return v3f(engine.Z, -engine.X, engine.Y);
}

v3f jointsOf(v3f turn)
{
	// in the chain's order: .z, .y, the bone's own (x)
	return v3f(turn.Z, turn.Y, turn.X) * RAD;
}

Json::Value frames(const physics::Bones &rest)
{
	const Json::Value still = quaternion(0, 0, 0, 1);
	Json::Value list(Json::arrayValue);
	list.append(frame("base", nullptr, "flu", v3f(0), still));
	physics::eachBone(rest, [&](const physics::Bone &bone) {
		std::string parent = bone.parent ? bone.parent : "base";
		for (size_t i = 0; i < 3; i++) {
			const std::string name = std::string(bone.name) + JOINTS[i].suffix;
			// the first joint is where the bone sits on its parent; the others turn in place
			Json::Value f = frame(name, parent.c_str(), "flu", i == 0 ? flu(bone.place.at) : v3f(0), still);
			f["joint"]["type"] = "continuous";
			f["joint"]["axis"] = vec(JOINTS[i].axis);
			if (i == 2 && bone.boxed) {
				f["shape"]["kind"] = "box";
				const v3f size = bone.size;
				f["shape"]["size"] = vec(v3f(size.Z, size.X, size.Y));
				f["shape"]["place"]["at"] = vec(flu(bone.centre));
				f["shape"]["place"]["turn"] = still;
			}
			list.append(f);
			parent = name;
		}
	});
	// the eye: mid-face, looking out of the head (rdf on flu: x right, y down, z ahead)
	const float head = physics::HEAD_SIZE * physics::FIGURE_SCALE;
	list.append(frame("eye", "head", "rdf", flu(v3f(0, head / 2, head / 2)), quaternion(-0.5f, 0.5f, -0.5f, 0.5f)));
	return list;
}

Json::Value sense()
{
	Json::Value s;
	s["channel"] = "/joints";
	s["kind"] = "vector";
	s["profile"] = "sensor_msgs/JointState";
	s["rate"] = 20;
	physics::eachBone(physics::Bones{}, [&](const physics::Bone &bone) {
		for (const Joint &j : JOINTS) {
			Json::Value c;
			c["name"] = std::string(bone.name) + j.suffix;
			c["unit"] = "rad";
			s["components"].append(c);
		}
	});
	return s;
}

std::vector<float> angles(const physics::Bones &now)
{
	std::vector<float> out;
	physics::eachBone(now, [&](const physics::Bone &bone) {
		const v3f a = jointsOf(bone.place.turn);
		out.push_back(a.X);
		out.push_back(a.Y);
		out.push_back(a.Z);
	});
	return out;
}

} // namespace rig

} // namespace headless
