// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "course.h"
#include "exceptions.h"
#include "workshop/showcase.h"
#include <cmath>
#include <fstream>

namespace shell {

Json::Value Course::read(const std::string &path)
{
	std::ifstream in(path);
	Json::Value json;
	std::string errors;
	if (!in || !Json::parseFromStream(Json::CharReaderBuilder(), in, &json, &errors) || !json["moves"].isArray())
		throw BaseException("wiki: no moves in " + path + " (bun run stage) " + errors);
	return json["moves"];
}

Course::Course(const Json::Value &move, const std::vector<sandbox::StageBlock> &blocks, const Json::Value &machines) :
	m_x(move["size"][0].asInt()), m_y(move["size"][1].asInt()), m_z(move["size"][2].asInt()), m_table(blocks),
	m_start(move["start"][0].asDouble(), move["start"][1].asDouble(), move["start"][2].asDouble()), m_end(move["end"].asDouble())
{
	m_blocks.assign((size_t)m_x * m_y * m_z, -1);
	m_heights.assign(m_blocks.size(), 1);
	for (const Json::Value &b : move["blocks"]) {
		const std::string node = b["node"].asString();
		int32_t index = -1;
		for (size_t i = 0; i < blocks.size(); i++)
			if (blocks[i].spec.node == node)
				index = (int32_t)i;
		if (index < 0)
			throw BaseException("wiki: move " + move["id"].asString() + " wants a block the palette hasn't: " + node);
		const Json::Value &from = b["from"], &to = b["to"];
		for (int y = from[1].asInt(); y <= to[1].asInt(); y++)
			for (int z = from[2].asInt(); z <= to[2].asInt(); z++)
				for (int x = from[0].asInt(); x <= to[0].asInt(); x++) {
					if (x < 0 || y < 0 || z < 0 || x >= m_x || y >= m_y || z >= m_z)
						throw BaseException("wiki: move " + move["id"].asString() + " has a block outside its size");
					m_blocks[((size_t)y * m_z + z) * m_x + x] = index;
					m_heights[((size_t)y * m_z + z) * m_x + x] = b.get("height", 1).asFloat();
				}
	}
	if (move.isMember("machine")) {
		const Json::Value &machine = machines[move["machine"].asString()];
		m_assembly = workshop::assemblyOf(machine);
		m_looks = machine["looks"];
		for (const Json::Value &c : move["drives"]) {
			physics::Driven::Cue cue;
			cue.at = c["at"].asFloat();
			cue.drive = { c.get("forward", 0).asFloat(), c.get("right", 0).asFloat(), c.get("brake", 0).asFloat() };
			cue.say = c.get("say", "").asString();
			m_drives.push_back(cue);
		}
	}
	for (const Json::Value &c : move["cues"]) {
		physics::Actor::Cue cue;
		cue.at = c["at"].asFloat();
		cue.ahead = c.get("ahead", 0).asFloat();
		cue.right = c.get("right", 0).asFloat();
		cue.yaw = c.get("yaw", 0).asFloat() * core::DEGTORAD;
		cue.run = c["run"].asBool();
		cue.jump = c["jump"].asBool();
		cue.crouch = c["crouch"].asBool();
		cue.prone = c["prone"].asBool();
		m_cues.push_back(cue);
	}
	m_ground_json = move["ground"];
	restart();
}

void Course::makeGround()
{
	m_loose.reset();
	if (m_ground_json.isNull())
		return;
	const Json::Value &g = m_ground_json;
	const std::string node = g["node"].asString();
	const sandbox::StageBlock *block = nullptr;
	for (const sandbox::StageBlock &b : m_table)
		if (b.spec.node == node)
			block = &b;
	if (!block || block->pile[2] == 0)
		throw BaseException("wiki: " + node + " isn't loose ground in the palette");
	Loose l;
	l.lo = v3s16(g["from"][0].asInt(), g["y"].asInt(), g["from"][1].asInt());
	l.nodes_x = g["to"][0].asInt() - l.lo.X + 1;
	l.nodes_z = g["to"][1].asInt() - l.lo.Z + 1;
	l.deep = g["deep"].asInt();
	l.block = block;
	l.field = std::make_unique<pile::Field>(l.nodes_x * pile::UNIT, l.nodes_z * pile::UNIT,
			pile::Material{ block->pile[0], block->pile[1], block->pile[2], block->pile[3] });
	pile::Field &f = *l.field;
	for (int z = 0; z < f.depth(); z++)
		for (int x = 0; x < f.width(); x++)
			f.set(x, z, 0, l.deep);
	// a hole dug: columns (x0, z0, x1, z1, how deep it goes), and it's left to stand or slump
	for (const Json::Value &h : g["holes"]) {
		for (int z = h[1].asInt(); z <= h[3].asInt(); z++)
			for (int x = h[0].asInt(); x <= h[2].asInt(); x++)
				f.set(x, z, 0, std::max(0, l.deep - h[4].asInt()));
		for (int z = std::max(0, h[1].asInt() - 1); z <= std::min(f.depth() - 1, h[3].asInt() + 1); z++)
			for (int x = std::max(0, h[0].asInt() - 1); x <= std::min(f.width() - 1, h[2].asInt() + 1); x++)
				f.stir(x, z);
	}
	m_loose = std::move(l);
	m_ground_began = groundRange();
}

std::array<int, 2> Course::groundRange() const
{
	if (!m_loose)
		return { 0, 0 };
	std::array<int, 2> out{ 1 << 30, -(1 << 30) };
	for (int z = 0; z < m_loose->field->depth(); z++)
		for (int x = 0; x < m_loose->field->width(); x++) {
			out[0] = std::min(out[0], m_loose->field->top(x, z));
			out[1] = std::max(out[1], m_loose->field->top(x, z));
		}
	return out;
}

// As pile::Piles: pressed once per sixteenth travelled, slid a step every twentieth of a second.
void Course::press(int who, v3f at, float radius, float kpa)
{
	Loose &l = *m_loose;
	const auto last = l.last.find(who);
	if (last != l.last.end() && last->second.getDistanceFrom(at) < 1.f / pile::UNIT)
		return;
	l.last[who] = at;
	const float top = l.lo.Y - 0.5f + (float)l.deep / pile::UNIT;
	if (std::fabs(at.Y - top) > 0.4f)
		return; // in the air
	// its columns from the node centres' grid
	l.field->press((at.X + 0.5f - l.lo.X) * pile::UNIT, (at.Z + 0.5f - l.lo.Z) * pile::UNIT, radius * pile::UNIT, kpa);
	l.version++;
}

void Course::pressAndSlide(float dt)
{
	constexpr float FOOT = 0.12f, PERSON = 800, PI = 3.14159265f, TICK = 0.05f; // as Server::pressGround, pile::Piles
	if (!m_loose)
		return;
	if (m_driven) {
		for (const physics::World::Contact &c : m_driven->contacts()) {
			const float r = c.width / 2;
			press((int)c.wheel, v3f::from(c.at), r, c.load / (PI * r * r) / 1000);
		}
	} else {
		press(-1, m_actor->at(), FOOT, PERSON / (PI * FOOT * FOOT) / 1000);
	}
	m_loose->behind += dt;
	while (m_loose->behind >= TICK) {
		m_loose->behind -= TICK;
		if (m_loose->field->step())
			m_loose->version++;
	}
}

std::optional<Scene::Ground> Course::ground() const
{
	if (!m_loose)
		return std::nullopt;
	const Loose &l = *m_loose;
	Scene::Ground out;
	out.corner = v3f(l.lo.X - m_x / 2.0f, (float)l.lo.Y, l.lo.Z - m_z / 2.0f);
	out.width = l.field->width();
	out.depth = l.field->depth();
	out.top.resize((size_t)out.width * out.depth);
	for (int z = 0; z < out.depth; z++)
		for (int x = 0; x < out.width; x++)
			out.top[(size_t)z * out.width + x] = l.field->top(x, z);
	out.tiles = l.block->tiles;
	out.version = l.version;
	return out;
}

int32_t Course::at(v3s16 n) const
{
	if (n.X < 0 || n.Y < 0 || n.Z < 0 || n.X >= m_x || n.Y >= m_y || n.Z >= m_z)
		return -1;
	return m_blocks[((size_t)n.Y * m_z + n.Z) * m_x + n.X];
}

void Course::restart()
{
	makeGround();
	const physics::Actor::Boxes solid = [this](v3s16 n, std::vector<aabb3f> &out) { boxes(n, out); };
	if (m_assembly) {
		// the block under a tyre says how it grips; loose ground is one material
		const auto surface = [this](const v3d &p) {
			const int32_t block = at(v3s16(std::floor(p.X + 0.5), std::floor(p.Y + 0.5), std::floor(p.Z + 0.5)));
			if (m_loose && p.X + 0.5 >= m_loose->lo.X && p.Z + 0.5 >= m_loose->lo.Z && p.X + 0.5 < m_loose->lo.X + m_loose->nodes_x
					&& p.Z + 0.5 < m_loose->lo.Z + m_loose->nodes_z)
				return m_loose->block->friction / physics::Wheel::GRIP;
			return block >= 0 ? m_table[block].friction / physics::Wheel::GRIP : 1.f;
		};
		m_driven = std::make_unique<physics::Driven>(solid, surface, *m_assembly, m_start, m_drives);
		return;
	}
	m_actor = std::make_unique<physics::Actor>(solid, m_start, m_cues);
}

void Course::boxes(v3s16 n, std::vector<aabb3f> &out) const
{
	// loose ground meets what moves as level as it was built: ruts are drawn, not ridden in (the game's are)
	if (m_loose && n.Y == m_loose->lo.Y && n.X >= m_loose->lo.X && n.Z >= m_loose->lo.Z && n.X < m_loose->lo.X + m_loose->nodes_x
			&& n.Z < m_loose->lo.Z + m_loose->nodes_z) {
		out.emplace_back(-0.5f, -0.5f, -0.5f, 0.5f, m_loose->deep / (float)pile::UNIT - 0.5f, 0.5f);
		return;
	}
	const int32_t block = at(n);
	if (block < 0 || m_table[block].spec.matter == "air")
		return;
	const auto &parts = m_table[block].boxes;
	if (parts.empty())
		out.emplace_back(-0.5f, -0.5f, -0.5f, 0.5f, m_heights[((size_t)n.Y * m_z + n.Z) * m_x + n.X] - 0.5f, 0.5f);
	for (const auto &b : parts) // (from its corner, there)
		out.emplace_back(b[0] - 0.5f, b[1] - 0.5f, b[2] - 0.5f, b[3] - 0.5f, b[4] - 0.5f, b[5] - 0.5f);
}

v3f Course::toScene(v3f at) const
{
	// a node's centre is on the integers here; in the scene its corner is
	return v3f(at.X + 0.5f - m_x / 2.0f, at.Y + 0.5f, at.Z + 0.5f - m_z / 2.0f);
}

double Course::time() const { return m_driven ? m_driven->time() : m_actor->time(); }
float Course::pace() const { return m_driven ? m_driven->pace() : m_actor->pace(); }

v3f Course::centre() const
{
	return toScene(m_driven ? v3f::from(m_driven->pose().position) : m_actor->at());
}

std::optional<Scene::Machine> Course::machine() const
{
	if (!m_driven)
		return std::nullopt;
	const physics::Pose root = m_driven->pose();
	Scene::Machine out;
	out.root = { toScene(v3f::from(root.position)), root.rotation };
	for (const physics::Relative &r : m_driven->relatives())
		out.bodies.push_back({ r.offset, r.rotation });
	return out;
}

Scene::Looks Course::looks() const
{
	Scene::Looks out;
	for (const Json::Value &body : m_looks) {
		std::vector<Scene::LookBox> parts;
		for (const Json::Value &p : body) {
			const Json::Value &b = p["box"];
			parts.push_back({ aabb3f(b[0].asFloat(), b[1].asFloat(), b[2].asFloat(), b[3].asFloat(), b[4].asFloat(), b[5].asFloat()),
				{ p["tiles"][0].asString(), p["tiles"][1].asString(), p["tiles"][2].asString() } });
		}
		out.push_back(std::move(parts));
	}
	return out;
}

void Course::update(float dt)
{
	if (!running)
		return;
	if (m_driven)
		m_driven->update(dt * (float)speed);
	else
		m_actor->update(dt * (float)speed);
	pressAndSlide(dt * (float)speed);
	if (time() >= m_end)
		restart();
}

Scene::Frame Course::frame() const
{
	Scene::Frame out{ m_x, m_y, m_z, {} };
	out.cells.resize(m_blocks.size());
	for (size_t i = 0; i < m_blocks.size(); i++) {
		out.cells[i].block = m_blocks[i];
		out.cells[i].temperature = 293;
		out.cells[i].level = m_heights[i];
	}
	return out;
}

std::optional<Scene::Figure> Course::figure() const
{
	if (m_driven)
		return std::nullopt;
	return Scene::Figure{ m_actor->bones(), toScene(m_actor->at()), m_actor->yaw() };
}

Json::Value Course::state() const
{
	Json::Value out;
	out["time"] = time();
	out["pace"] = pace();
	if (m_driven) {
		out["doing"] = m_driven->doing();
		out["mass"] = m_driven->mass();
		return out;
	}
	out["doing"] = m_actor->doing();
	return out;
}

} // namespace shell
