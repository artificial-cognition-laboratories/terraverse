// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "orbit/field.h"

#include "irr_v3d.h"
#include "mapnode.h"
#include "physics/structure.h"
#include "physics/world.h"
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

class IGameDef;
class Map;
class NodeDefManager;
class LuaEntitySAO;
class ServerEnvironment;
struct MapEditEvent;
namespace piece { class Machine; }

namespace workshop {

struct StructureQuestion;
class Laying;

// What a block is as a thing that moves: from its matter (Lua says, by
// core.physics.material).
// What a block does in a machine besides weigh (sprint 69): nothing, a seat
// (sat on, its sitter drives), a motor (an engine: turns its machine's
// driven wheels). A block with a role is a fitting (sprint 73): fitted whole,
// facing a way, never drawn in sixteenths.
enum class Role : u8 { None, Seat, Motor, Count };
struct RoleKind
{
	const char *id; // Lua's name for it
};
extern const RoleKind ROLES[size_t(Role::Count)];

struct Material
{
	float density;  // kg/m³
	float friction; // Coulomb
	Role role = Role::None;
	// a motor's (sprint 78): its power at full throttle (W) and how fast
	// it turns at most (rpm); an engine's a fitting, not a block of iron
	float power = 0, rpm = 0;
	// what it takes before it fails (Pa): pulled apart or sheared, and crushed.
	// 0: none declared, and it isn't what fails (physics::StructureCell)
	float tension = 0, compression = 0;
	float toughness = 0; // J/m² a crack through it costs
};

// How a machine is held (TOSERVER_OBJECT_HOLD's u8): not at all, still
// where its holder poses it (the workbench), or carried (pulled toward the
// hands as hard as the carrier can: sprint 58).
enum class Grip : u8 { Free = 0, Still = 1, Carried = 2 };

// What TOSERVER_CLIPBOARD asks (sprint 74), by its u8.
enum class Clip : u8 { Copy, Paste, Take, Forget, Pull, Count };

// How a new object is handed to its player (TOCLIENT_HELD's u8): focused
// on the bench; carried till the next click (a paste); carried while the
// button that pulled it off is down (sprint 77).
enum class Handed : u8 { Focused, Carried, Pulled, Count };

// How hard a player pulls what they carry (N): in creative, as hard as
// they like; else as a strong person (about 150 kg held up).
constexpr float STRENGTH_CREATIVE = 1e6f, STRENGTH = 1500;

// The world's blocks in map block `block` as boxes (nodes, from its least
// node's centre), as things that move meet them: cut and smoothed shapes as
// they're drawn, whole blocks inside the ground left out. False: it isn't
// loaded. The server's physics and a client's use it alike.
bool groundBoxes(Map &map, const NodeDefManager *ndef, v3s16 block, std::vector<aabb3f> &boxes);

// Every block's material, by content id: declared as mods load, before
// there's a world to move (Server owns it; Motion reads it).
class Materials
{
public:
	// Throws std::invalid_argument on a material that isn't one.
	void set(content_t c, const Material &m);
	const Material *get(content_t c) const;
	// Each content id that has one, with it.
	std::vector<std::pair<content_t, Material>> all() const;

private:
	std::vector<std::optional<Material>> m_by_content;
};

/*
	Motion (sprint 57): machines moved by physics. Each machine entity in
	the world is an assembly of physics::World: each rigid group of its
	parts (Machine::rigid) a body of its built blocks' boxes, weighed by
	their materials; each hinge a hinge, its friction rolling resistance.
	A hinged group that's round about its axle rolls as a smooth cylinder.

	It drives its entities (LuaEntitySAO::setDriven): their position,
	turn and velocity are the physics', and their groups' poses go to
	clients (AO_CMD_SET_PARTS). A machine edited is rebuilt where it is;
	held, it's kinematic, where its holder poses it. Players push.
*/
// The physics' field from the planet's (it's kept: the field outlives the world).
physics::Field fieldOf(const orbit::Field &field);

class Motion
{
public:
	Motion(ServerEnvironment &env, IGameDef *gamedef, const Materials &materials, const orbit::Field &field);
	~Motion();

	// Every machine in the world found, built, stepped and drawn there.
	void step(float dtime);
	void mapEdited(const MapEditEvent &e);

	// Its entity's machine is held (still, posed by its holder) or let go.
	void hold(LuaEntitySAO &obj, bool held);
	bool held(const LuaEntitySAO &obj) const;
	// Carried (pulled as hard as `strength`, N) or not (none).
	void carry(LuaEntitySAO &obj, std::optional<float> strength);
	bool carried(const LuaEntitySAO &obj) const;
	// Where a held or carried one's to be (BS, degrees); throws if it's neither.
	void pose(LuaEntitySAO &obj, v3f position, v3f rotation);
	// Grabbed by `who` (their object id) at `point` (BS, in its frame) as
	// hard as `strength` (N), that point pulled toward `hand` (BS); or
	// let go (none). Sprint 68.
	struct Grab
	{
		v3f point;
		v3f hand;
		float strength;
	};
	void grab(LuaEntitySAO &obj, u16 who, std::optional<Grab> grab);
	// Whether `who` has it grabbed.
	bool grabbing(const LuaEntitySAO &obj, u16 who) const;
	// Driven by `who` (their object id) so, or got up from (none): a
	// driver isn't a body in its way (sprint 69). The machine's seat, if
	// it has one (BS, in its frame).
	void drive(LuaEntitySAO &obj, u16 who, std::optional<physics::Drive> drive);
	std::optional<u16> driver(const LuaEntitySAO &obj) const;
	std::optional<v3f> seat(u16 object) const;

	// For core.physics: nothing if it isn't a machine physics moves.
	std::optional<v3f> velocity(u16 object) const; // BS/s
	std::optional<float> mass(u16 object) const;   // kg
	// Every rolling machine's wheels on the ground (sprint 84: they press
	// loose ground), by object; contact points in nodes.
	struct WheelOn
	{
		u16 object;
		physics::World::Contact contact;
	};
	std::vector<WheelOn> wheelsOn() const;
	// kg·BS/s, from where its giver stands (BS) if it's given; throws on
	// what isn't one
	void impulse(u16 object, v3f impulse, std::optional<v3f> from = std::nullopt);
	// Set back on its feet, lifted `lift` (nodes): physics::World::upright.
	void upright(LuaEntitySAO &obj, float lift);
	// A blast at `centre` (nodes), `power` Pa·s at a node off. The pulse: objects
	// flung and loaded by what reaches each cell, the ground cut as far as it
	// overcomes what each block takes. The throw: chunks of what was cut, real
	// objects, flying. The fallout: the rest laid round it as a rim. Fire's Lua's.
	// What it mostly dug, and how far (for what Lua shows of it): none if nothing.
	struct Crater
	{
		content_t material = CONTENT_AIR;
		float radius = 0;
	};
	Crater blast(v3f centre, float power);

private:
	struct Tracked
	{
		physics::World::Id id;
		size_t text;    // its piece text's hash: when it changes, it's rebuilt
		std::optional<v3f> seat; // its driver's (nodes, its frame)
		std::string parts_sent;
		bool resting = false; // it had come to rest, and was asked whether it holds
	};
	// What came to rest holds itself up and what's taken a blow survives it, or
	// it's cut into what does. The solve's off the server's thread, one at a time;
	// its answer counts only if the thing's as it was asked about.
	void settle(const std::map<u16, LuaEntitySAO *> &seen);
	struct Judging
	{
		u16 object;
		size_t text;
		bool blow; // (a hand doesn't stop a blow counting; it does stop a rest)
		std::shared_ptr<StructureQuestion> question;
	};
	std::optional<Judging> m_judging;
	std::future<physics::StructureResult> m_answer;
	std::vector<Judging> m_to_judge; // asked as it happened, waiting their turn
	std::set<u16> m_rubble; // broken off something: small and weak, it crumbles where it stops
	bool crumble(u16 id, LuaEntitySAO &e, physics::World::Id body);
	// A blast's rim, laid a little each step: it comes down as fallout, not as a stall.
	struct Fallout
	{
		std::unique_ptr<Laying> laying;
		struct Column
		{
			s32 x, z;
			int height;
		};
		std::vector<Column> columns;
		size_t next = 0;
		s32 top;
		int grain;
		content_t material;
	};
	std::vector<Fallout> m_fallout;
	void fall();

	bool ground(v3s16 block, std::vector<aabb3f> &boxes);
	Tracked *tracked(u16 object);

	ServerEnvironment &m_env;
	IGameDef *m_gamedef;
	std::unique_ptr<physics::World> m_world;
	const Materials &m_materials;
	std::map<u16, Tracked> m_tracked;                 // by object id
	std::map<u16, size_t> m_broken;                   // can't be built (its text's hash): said once
	std::set<u16> m_held;                             // held, tracked yet or not
	std::map<u16, float> m_carried;                   // carried, how hard, tracked yet or not
	std::map<u16, std::map<u16, Grab>> m_grabbed;     // grabbed, by whom, tracked yet or not
	struct Driven
	{
		u16 who;
		physics::Drive drive;
	};
	std::map<u16, Driven> m_driven;                   // driven, by whom, tracked yet or not
	void grip(u16 object, u16 who, const Grab &g, bool fresh);
};

// A fitting's front (sprint 73) from its facedir (the level four of it),
// and the facedir nearest a level look.
v3s16 facing2(u8 param2);
u8 facedirOf(v3f look);
// Where a fitting set flat on the face of sixteenth `at` (outward `normal`)
// has its least sixteenth: against the face along it, on the block grid
// across it. The client outlines what the server fits.
v3s32 fittingCorner(v3s32 at, v3s16 normal);

// How well tyres grip the node under a place (physics::World::Opts::surface): its
// material's friction against an ordinary block's.
float surfaceGrip(Map &map, const NodeDefManager *ndef, const Materials &materials, const v3d &at);

// A machine compiled as physics sees it (its frame: nodes, from its node
// (0, 0, 0)'s centre): each rigid group a body of its built blocks' boxes,
// each hinge between groups a hinge (a round group about it rolls as a
// cylinder). Throws std::runtime_error naming a block with no material.
physics::Assembly compile(const piece::Machine &m, const Materials &materials, IGameDef *gamedef);

// A turn as the server sets an entity's (degrees: piece::frame) and back.
physics::Quat quatOf(v3f rotation);
// v turned by q
v3f turned(const physics::Quat &q, v3f v);
v3f rotationOf(const physics::Quat &q);

} // namespace workshop
