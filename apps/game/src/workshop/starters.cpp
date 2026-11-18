// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "starters.h"

#include "gamedef.h"
#include "nodedef.h"
#include "sculpt/sculpt.h"
#include <map>
#include <stdexcept>

namespace workshop {

const Starter STARTERS[5] = {
	{ "Wheel", std::nullopt },
	{ "Sprung corner", piece::Joint::Rotary },
	{ "Steering corner", piece::Joint::Steered },
	{ "Driven corner", piece::Joint::Driven },
	{ "Steering driven corner", piece::Joint::SteerDriven },
};

namespace {

content_t material(IGameDef *gamedef, const char *name)
{
	content_t id;
	if (!gamedef->ndef()->getId(name, id))
		throw std::runtime_error(std::string("starter parts need ") + name);
	return id;
}

// a wheel's tyre: a cylinder RADIUS sixteenths round, WIDTH wide, out of
// the +x face of the sixteenth `at` (as a stroke dragged from its middle)
constexpr s32 RADIUS = 7, WIDTH = 4;
sculpt::Stroke tyre(const shape::Sixteenth &at)
{
	sculpt::Stroke s{ sculpt::Tool::Cylinder, WIDTH, at, v3s16(1, 0, 0), sculpt::Act::Add };
	s.to = sculpt::local(sculpt::world(at) + v3s32(0, RADIUS, RADIUS));
	return s;
}

} // namespace

piece::Machine starter(IGameDef *gamedef, const std::string &name)
{
	const Starter *which = nullptr;
	for (const Starter &s : STARTERS)
		if (name == s.name)
			which = &s;
	if (!which)
		throw std::out_of_range("no starter part \"" + name + "\"");
	const content_t steel = material(gamedef, "materials:steel"), rubber = material(gamedef, "materials:rubber");
	piece::Machine m(gamedef);
	// what it's grown from: a hub, six sixteenths of steel in its node's middle
	piece::Object hub(gamedef);
	shape::Shape cube;
	for (s16 x = 5; x < 11; x++)
		for (s16 y = 5; y < 11; y++)
			for (s16 z = 5; z < 11; z++)
				cube.set({ x, y, z }, true);
	hub.design().map().setNode({ 0, 0, 0 }, MapNode(steel));
	hub.design().map().setShape({ 0, 0, 0 }, cube);
	hub.build();
	const u64 h = m.add({ std::move(hub), piece::Pose{ v3s32(0), 0 } }).object.id();
	// the wheel off its +x face, at its middle
	const piece::Machine::Face face{ h, { v3s16(0, 0, 0), v3s16(10, 7, 7) }, v3s16(1, 0, 0) };
	const u64 wheel = m.grow(face, tyre(face.at).cells(), rubber, which->corner.value_or(piece::Joint::Rotary)).object.id();
	m.build();
	// a wheel alone: the hub was only to grow it from; carried onto a face
	// it turns there, as it hung from the hub
	if (!which->corner)
		return m.extract(wheel);
	// a corner's carried onto a chassis on its spring
	// by its hub's inner face, the wheel's side across from it (sprint 77)
	const piece::Machine::Face inner{ h, { v3s16(0, 0, 0), v3s16(5, 7, 7) }, v3s16(-1, 0, 0) };
	m.setPlug(piece::Machine::Plug{ piece::Joint::Sprung, piece::Spring{},
		piece::Machine::Plug::By{ m.point(inner), inner.normal } });
	return m;
}

std::vector<std::string> starterNames()
{
	std::vector<std::string> out;
	for (const Starter &s : STARTERS)
		out.push_back(s.name);
	return out;
}

} // namespace workshop
