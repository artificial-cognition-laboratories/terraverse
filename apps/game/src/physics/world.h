// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_aabb3d.h"
#include "irr_v3d.h"
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

/*
	Physics (sprint 57): rigid bodies on Jolt Physics. A pure module: it
	knows boxes, densities, hinges and ground, never a machine, a node or
	an entity (the workshop compiles those into it).

	Units are the world's nodes (a node is a metre), the world's frame
	(node centres at whole numbers), seconds and kilograms. Positions are
	doubles: Jolt is built in double precision.

	An assembly is what a machine becomes: bodies (each a rigid group of
	parts) and hinges between them, all given in the assembly's own frame,
	whose origin and turn are its pose. Its bodies never collide with each
	other. It's Live (simulated) or Frozen (kept as a pose, out of Jolt):
	the tier (planning/simulation-scales.md). Held, it's kinematic: moved
	where its holder says, pushing what it meets.

	Characters (sprint 58) are Jolt's virtual characters: capsules that walk
	by collide-and-slide, up steps, over slopes, standing on (and carried by)
	what moves, pushing bodies as hard as they're strong. A character moves
	at the velocity it's given (its controller's: physics/walk.h), or is
	placed (a player whose client moves them: it pushes on its way there).
	Carried, an assembly is pulled toward where it's to be as hard as its
	carrier can: light things follow, heavy ones lag or drag.
	Grabbed (sprint 68), a point of it is pulled toward a hand as hard as
	its grabber can: a light thing comes up, a heavy one turns about the
	point or tips, what's too heavy barely gives. Many can grab one.

	Gravity and air are a field (sprint 59), asked at each body and
	character every step: a planet's pull points at its centre and weakens
	with height, its air drags on everything that moves.

	Ground is asked of the caller a map block (16³ nodes) at a time, and
	only round Live assemblies and characters.

	Deterministic: one thread, a fixed step; the same calls give the same
	bodies, bit for bit.
*/
namespace physics {

struct PhysicsError : std::runtime_error
{
	using std::runtime_error::runtime_error;
};

constexpr int BLOCK = 16;     // nodes a side of a block of ground
constexpr double STEP = 1.0 / 60;

struct Box
{
	aabb3f box;     // nodes, its body's frame
	float density;  // kg/m³
	float friction; // Coulomb, 0..1+
};

// A smooth cylinder a body collides as instead of its boxes (a wheel: a
// round part on a hinge rolls true, not lumpy). Its mass is still its
// boxes'.
struct Round
{
	v3f centre, axis; // axis a unit vector
	float radius, half_width;
};

struct Body
{
	std::vector<Box> boxes;
	std::optional<Round> round;
};

struct Hinge
{
	size_t a, b;   // bodies
	v3f anchor;    // the assembly's frame
	v3f axis;      // unit
	float friction; // N·m that turning it costs (rolling resistance)
	float limit = 0; // radians either way it turns; 0: no stops
};

// A wheel (sprint 66): a round body on an axle across the root, rolled as
// a vehicle's wheel is (a suspension, a tyre's grip, its axle's friction)
// rather than as a free body on a hinge, which chatters on the ground.
// Its body follows it, there to be touched, never meeting the ground.
struct Wheel
{
	size_t body;
	v3f centre;     // the assembly's frame
	v3f axis;       // unit, level in the root's frame
	float radius, half_width;
	float friction; // N·m that turning it costs (rolling resistance)
	// its joint's (sprint 73): turned by the driver's left and right, by
	// the engines
	bool steers = false, driven = false;
	// its tread's friction (sprint 74: its rim's blocks'; a rubber tyre's
	// grips): its tyre grips as Jolt's car tyre does at GRIP, more or less
	// with more or less
	float grip = GRIP;
	static constexpr float GRIP = 0.6f; // most blocks'
	// its suspension (sprint 75): how far it travels all told, and how far
	// its spring's sagged where it was built (nodes); a wheel built on no
	// spring has a little, stiff, so it rides over a sixteenth's step
	float travel = 0.1f, sag = 0.05f;
};

// A hub (sprint 76): a body on a spring off the root that a wheel turns on.
// It follows its wheel's suspension, not its turn: the strut's seen to
// work. With no wheel (none on it yet) it rides the root as if welded.
struct Hub
{
	size_t body;
	std::optional<size_t> wheel; // which of the assembly's wheels
};

struct Assembly
{
	std::vector<Body> bodies; // the first is its root: its pose is the assembly's
	std::vector<Hinge> hinges;
	std::vector<Wheel> wheels; // all on one axis, each a body none of the hinges join
	std::vector<Hub> hubs;
	// Driving (sprint 69): where its driver sits (the assembly's frame,
	// nodes: a seat's top) and which way the seat faces (sprint 73: ahead,
	// a level unit vector), and its engines' power (W) and how fast they
	// turn at most (rpm: sprint 78, a power, not a torque)
	std::optional<v3f> seat;
	v3f facing{ 0, 0, 1 };
	float power = 0, rpm = 0;
};

// Where it's heavy and where it's lifted (sprint 103), the assembly's frame:
// what a builder needs to see. It flies nose first if the mass is ahead of the lift.
struct Balance
{
	float mass = 0;
	v3f mass_at;
	bool lifts = false;
	v3f lift_at;
};
Balance balanceOf(const Assembly &a);

// A driver's hands and feet (sprint 69): forward (-1 back .. 1), right
// (-1 left .. 1), brake (0 .. 1). Forward is the seat's facing (sprint 73).
struct Drive
{
	float forward = 0, right = 0, brake = 0;
};

struct Quat
{
	float x = 0, y = 0, z = 0, w = 1;
	bool operator==(const Quat &o) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
};

struct Pose
{
	v3d position;
	Quat rotation;
	bool operator==(const Pose &o) const { return position == o.position && rotation == o.rotation; }
};

// A body's pose in its assembly's root's frame: how far a wheel has turned.
struct Relative
{
	v3f offset;
	Quat rotation;
};

enum class Tier : unsigned char {
	Live,   // in Jolt, stepped
	Frozen, // at rest and away from players: a pose, costing nothing
};

struct Capsule // a player as the server has them: placed where their client says
{
	v3d foot;   // the bottom of it
	float radius, height;
	float mass = 75, strength = 600; // kg; N they can push with
};

struct CharacterSpec
{
	float radius, height; // nodes: a capsule standing on its foot
	float mass;           // kg
	float strength;       // N it pushes with
	float max_slope;      // degrees it can stand on
};

// One part of a jointed body (sprint 105): a box, or a capsule along its y.
struct LimbSpec
{
	v3f half;         // box half sizes; a capsule's: radius in x, half its straight part in y
	bool capsule = false;
	float mass = 1;   // kg
	int parent = -1;  // joined to this part
	v3f joint;        // where, in this part's frame
	float bends_from = 0, bends_to = 0; // about x, radians, from where it stands when built: a hinge. Equal: a ball joint
	float cone = 1;   // a ball joint's swing, radians, about...
	v3f cone_axis{ 0, -1, 0 }; // ...this, as built: off its length, it swings further one way than the other
	std::vector<int> passes; // earlier parts it needn't keep out of, besides its parent
	float strength = 100;     // N·m its joint's muscles can turn it with, at most
};

// What a place does to what's in it (sprint 59: gravity is a field).
struct Here
{
	v3f gravity; // nodes/s²
	float air;   // kg/m³: drag on what moves through it
};
// At a position, moving so (the turning frame's pull depends on it).
using Field = std::function<Here(const v3d &at, const v3f &velocity)>;
// The same everywhere, with no air (tests, and worlds that want it).
Field uniform(v3f gravity);

class World
{
public:
	struct Opts
	{
		Field field;
		// The ground in block `block`: boxes in nodes from its least node's
		// centre. False: it isn't loaded (what's on it stays frozen).
		std::function<bool(v3s16 block, std::vector<aabb3f> &boxes)> ground;
		// How near a player keeps an assembly Live (nodes); it freezes at
		// rest past `freeze` (more: so it doesn't flicker at the edge).
		// How well tyres grip the ground at a place, × their own (ordinary ground 1, ice much less). None: 1.
		std::function<float(const v3d &)> surface;
		float live = 48, freeze = 56;
		float step_up = 0.6f; // nodes a character walks up without jumping
	};
	explicit World(const Opts &opts);
	~World();

	using Id = unsigned;
	// Throws PhysicsError on an assembly with no bodies, a body with no
	// boxes, or a hinge naming a body it hasn't.
	Id add(const Assembly &a, const Pose &pose, v3f velocity = v3f(0));
	void remove(Id id);
	bool has(Id id) const;
	// Its bodies remade (it was edited): where it is and how it moves kept.
	void rebuild(Id id, const Assembly &a);

	// Held: kinematic, it goes where `to` says (each step, at the speed
	// that gets it there); let go, it falls from there as it was moving.
	void hold(Id id, bool held);
	void moveTo(Id id, const Pose &to);
	// Set back on its feet (sprint 79: a car on its roof): lifted `lift`
	// (nodes), level, facing as it faced, still. Wakes it.
	void upright(Id id, float lift);
	// N·s at its root's centre of mass; or, given who gives it (where they
	// stand), at the nearest point of the body nearest them (a corner pushed
	// turns it).
	void impulse(Id id, v3f impulse, std::optional<v3d> from = std::nullopt);

	// The players this step, each a character put where they are (pushing
	// what's in the way as hard as they can); Live/Frozen follows them.
	void players(const std::vector<Capsule> &players);

	// Carried (a strength, N) or let go (none); while carried, pulled toward
	// `to` each step. Being carried isn't being held: it's not kinematic.
	void carry(Id id, std::optional<float> strength);
	void pullTo(Id id, const Pose &to);
	bool carried(Id id) const;
	bool held(Id id) const;

	// Grabbed by `who` (anyone's key) at `point` (nodes, in its frame: the
	// pose's) as hard as `strength` (N); while grabbed, that point of the
	// body nearest it is pulled toward the hand each step.
	void grab(Id id, unsigned who, v3f point, float strength);
	void grabTo(Id id, unsigned who, const v3d &hand);
	void release(Id id, unsigned who);
	bool grabbed(Id id) const;
	// Where `who`'s grip is now (nodes): on the body it's on, as that's
	// turned (a wheel's turns with it); nothing if they've none.
	std::optional<v3d> gripAt(Id id, unsigned who) const;
	// Driven so, or not (none): its steering wheels steered, its engines
	// turning its driven ones (none, it has none or none's driven), braked.
	void drive(Id id, std::optional<Drive> drive);
	// Moved a share (0..1) of the way to `to`, all its bodies as one, moving
	// as they were: a copy brought toward another's word for it. Further
	// than `snap` (nodes), it's put there.
	void correct(Id id, const Pose &to, float share, float snap);

	using CharId = unsigned;
	CharId addCharacter(const CharacterSpec &spec, const v3d &foot);
	void removeCharacter(CharId id);
	// Made this tall (nodes), its foot where it is; false, and unchanged, if it'd not fit.
	bool resize(CharId id, float height);
	// The velocity it walks at from the next step (nodes/s, gravity its own).
	void want(CharId id, v3f velocity);
	// Whether ground that isn't loaded is air to it (something flying faster
	// than the map arrives), or waited for (walking: the default).
	void unknownIsAir(CharId id, bool air);
	void keepsSpeed(CharId id, bool keeps);
	void skipsGround(CharId id, bool skips);
	// Put somewhere, moving so (a teleport, or a hand-over from other movement).
	void place(CharId id, const v3d &foot, v3f velocity);
	struct CharacterState
	{
		v3d foot;
		v3f velocity;
		bool on_ground;      // standing on something it can stand on
		v3f ground_velocity; // of what it stands on (a cart it rides)
		v3f ground_normal;
		std::optional<Id> standing_on; // the assembly it stands on, if it's one
	};
	CharacterState character(CharId id) const;
	// What it's walked into held assemblies since last asked (N·s each,
	// as hard as it pushes): they don't give, so this is the push for
	// whoever moves them (sprint 61).
	std::vector<std::pair<Id, v3f>> pushes(CharId id);
	// Where it's drawn now: between its last two steps by the time since.
	v3d drawn(CharId id) const;
	// A block of ground changed: built again, what's on it woken.
	void groundChanged(v3s16 block);

	// A jointed body (sprint 105): parts that meet the ground, what moves and each other (not what they're joined to), not characters.
	// `built`: where each stands as its joints are measured. It's then put at `at`.
	using LimbsId = unsigned;
	LimbsId addLimbs(const std::vector<LimbSpec> &parts, const std::vector<Pose> &built, const std::vector<Pose> &at, v3f velocity);
	void removeLimbs(LimbsId id);
	// Each step it goes to these. `stiff`: how strongly its hips are guided to theirs (0 let go): the rest
	// follows by `tone`, each part turned against its parent as they'd be there (1 firmly, 0 limp).
	void driveLimbs(LimbsId id, const std::vector<Pose> &to, float stiff, float tone = 0);
	// A part's point (in its own frame) reaching for a place, with an arm's strength; there, it's held to it by a
	// joint till let go (sprint 105: grip). None: let go.
	// `over`: the way across what it holds (an edge's in from it): from below, it comes up and over, not into its face.
	void reachWith(LimbsId id, size_t part, v3f point, std::optional<v3d> to, v3f over = v3f(0));
	bool holding(LimbsId id, size_t part) const;
	// Its legs (or arms) drove it: every part's going this much faster at once.
	void launchLimbs(LimbsId id, v3f by);
	std::vector<Pose> limbs(LimbsId id) const;
	// How much of its guide it took last step to keep its hips where they're asked: past 1, it couldn't.
	float limbsGuided(LimbsId id) const;

	// Held where it's put (a body climbing: its hands have it): no floor pulls it down, no step lifts it.
	void holdsStill(CharId id, bool held);

	// Advances by `dt` in fixed steps (at most a fifth of a second's: a
	// slow server drops time rather than falling further behind).
	void step(double dt);

	Pose pose(Id id) const;
	v3f velocity(Id id) const;
	float mass(Id id) const;
	Tier tier(Id id) const;
	bool asleep(Id id) const;
	// The hardest blow its root has taken since last asked: what its velocity
	// (nodes/s) and spin (rad/s) did that its forces don't explain, in the world's
	// axes. So a hand or gravity is no blow, and a floor is. None: nothing to speak of.
	struct Blow
	{
		v3f velocity, spin;
	};
	std::optional<Blow> blow(Id id);
	// What pulls on it where it is (m/s²).
	v3f gravity(Id id) const;
	// Of these boxes (nodes, its frame), which meet the ground or anything else
	// not of it. For a Live assembly; none of them on a Frozen one.
	std::vector<bool> touching(Id id, const std::vector<aabb3f> &boxes) const;
	// Each body but the root, in order: where it is in the root's frame.
	std::vector<Relative> relatives(Id id) const;

	// For finding what's wrong (sprint 72): its tier and pose, each wheel's
	// contact (where, how far its suspension's out), and the ground blocks
	// under it (built, and how many boxes) — one line of words.
	std::string describe(Id id) const;

	// Where its wheels touch the ground (sprint 84: they press loose ground
	// into ruts): each touching wheel's index, contact point, the load on
	// it (N) and its tyre's width and radius. None unless Live and rolling.
	struct Contact
	{
		size_t wheel;
		v3d at;
		float load, width, radius;
	};
	std::vector<Contact> contacts(Id id) const;

	// How many ground blocks are built (for tests: only round what's Live).
	size_t groundBlocks() const;

private:
	struct Impl;
	std::unique_ptr<Impl> m;
};

} // namespace physics
