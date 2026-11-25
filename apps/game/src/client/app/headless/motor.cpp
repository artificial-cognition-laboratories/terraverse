// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/headless/motor.h"
#include <algorithm>
#include <cmath>

namespace headless {

namespace {

// One component a control takes.
struct Output
{
	const char *name;
	const char *unit;
	const char *control; // the interface: "interface" is a macro in Windows headers
	float low, high;
};

struct Control
{
	const char *channel;
	const char *frame; // whose axes, or "" for none
	std::vector<Output> outputs;
	std::vector<const char *> joints; // the frame each component moves, or null: the view's yaw turns the whole body, no joint
};

constexpr float DEG = 57.29578f;

Output switched(const char *name) { return { name, "bool", "switch", 0, 1 }; }

// The table every decision reads: the manifest, accepting, and the doing.
const std::vector<Control> &controls()
{
	static const std::vector<Control> table = {
		{ "/legs", "base", { switched("forward"), switched("back"), switched("left"), switched("right"),
			switched("jump"), switched("crouch"), switched("sprint") }, {} },
		{ "/look", "", { { "yaw", "rad/s", "velocity", -Motor::TURN, Motor::TURN },
			{ "pitch", "rad/s", "velocity", -Motor::TURN, Motor::TURN } }, { nullptr, "head" } },
		{ "/hands", "", { switched("dig"), switched("place") }, {} },
	};
	return table;
}

const Control *find(const std::string &channel)
{
	for (const Control &c : controls())
		if (channel == c.channel)
			return &c;
	return nullptr;
}

float clamped(const Output &o, float v)
{
	if (!std::isfinite(v))
		return 0;
	if (std::string(o.control) == "switch")
		return v >= 0.5f ? 1 : 0;
	return std::clamp(v, o.low, o.high);
}

} // namespace

Json::Value Motor::declared()
{
	Json::Value list(Json::arrayValue);
	for (const Control &c : controls()) {
		Json::Value control;
		control["channel"] = c.channel;
		control["kind"] = "vector";
		if (*c.frame)
			control["frame"] = c.frame;
		control["lapse"]["kind"] = "release";
		control["lapse"]["after"] = HOLD;
		control["rate"] = 20;
		for (size_t i = 0; i < c.outputs.size(); i++) {
			const Output &o = c.outputs[i];
			Json::Value item;
			item["name"] = o.name;
			item["unit"] = o.unit;
			item["interface"] = o.control;
			item["range"].append(o.low);
			item["range"].append(o.high);
			if (i < c.joints.size() && c.joints[i])
				item["joint"] = c.joints[i];
			control["components"].append(item);
		}
		list.append(control);
	}
	// a line said in the world's chat: taken whole, never held (Headless::heard sends it)
	Json::Value say;
	say["channel"] = "/say";
	say["kind"] = "text";
	list.append(say);
	return list;
}

std::optional<std::string> Motor::accept(const std::string &channel, unsigned long long seq,
		const std::vector<float> &values, unsigned long long now)
{
	const Control *c = find(channel);
	if (!c)
		return "there's no control on " + channel;
	if (values.size() != c->outputs.size())
		return channel + " takes " + std::to_string(c->outputs.size()) + " values, not " + std::to_string(values.size());
	std::vector<float> applied(values.size());
	for (size_t i = 0; i < values.size(); i++)
		applied[i] = clamped(c->outputs[i], values[i]);
	m_held[channel] = Held{ applied, seq, now + HOLD, false };
	return std::nullopt;
}

Motor::Step Motor::step(float dtime, unsigned long long now)
{
	Step out;
	auto held = [&](const char *channel) -> const std::vector<float> * {
		auto it = m_held.find(channel);
		if (it == m_held.end())
			return nullptr;
		if (now > it->second.until) {
			m_held.erase(it); // lapsed: let go
			return nullptr;
		}
		if (!it->second.answered) {
			out.applied.push_back({ channel, it->second.seq, it->second.values });
			it->second.answered = true;
		}
		return &it->second.values;
	};

	const std::vector<float> *legs = held("/legs"), *look = held("/look"), *hands = held("/hands");
	if (look) {
		// flu: a turn about z (up) + is to the left, about y (left) + tips the nose down; the
		// engine keeps yaw + left and pitch + down, so both carry straight over
		m_yaw += (*look)[0] * DEG * dtime;
		m_pitch = std::clamp(m_pitch + (*look)[1] * DEG * dtime, -90.0f, 90.0f);
	}
	auto on = [](const std::vector<float> *v, size_t i) { return v && (*v)[i] >= 0.5f; };
	out.control = PlayerControl(
		on(legs, 0) ? 1.0f : 0.0f, on(legs, 1) ? 1.0f : 0.0f, on(legs, 2) ? 1.0f : 0.0f, on(legs, 3) ? 1.0f : 0.0f,
		/* jump */ on(legs, 4), /* aux1, sprinting */ on(legs, 6), /* sneak, crouched */ on(legs, 5),
		/* zoom */ false,
		/* dig */ on(hands, 0), /* place */ on(hands, 1),
		m_pitch, m_yaw);
	out.control.setMovementFromKeys();
	return out;
}

void Motor::face(float yaw, float pitch)
{
	m_yaw = yaw;
	m_pitch = pitch;
}

} // namespace headless
