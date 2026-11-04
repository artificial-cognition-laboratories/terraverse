// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "physics/actor.h"
#include "physics/driven.h"
#include "pile/pile.h"
#include "scene.h"
#include <json/json.h>
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace shell {

/*
	One of the wiki's Movement scenes (game/moves.json): a few of the game's
	blocks and which keys are down when, played by a physics::Actor. Starts
	again when it ends, so a move can be watched as many times as it takes.
*/
class Course
{
public:
	static Json::Value read(const std::string &path); // its moves
	// `machines`: the palette's baked ones, for a scene that drives one.
	Course(const Json::Value &move, const std::vector<sandbox::StageBlock> &blocks, const Json::Value &machines);

	void update(float dt);
	void restart();
	Scene::Frame frame() const;
	std::optional<Scene::Figure> figure() const;
	std::optional<Scene::Machine> machine() const; // none: a body plays it
	Scene::Looks looks() const; // a driven machine's
	v3f centre() const; // what the camera follows
	double time() const;
	float pace() const;
	Json::Value state() const;
	const physics::Actor &actor() const { return *m_actor; }
	const physics::Driven *driven() const { return m_driven.get(); }
	double end() const { return m_end; }
	// Loose ground in it (sixteenths): where it's lowest and highest now, and where it started.
	std::optional<Scene::Ground> ground() const;
	std::array<int, 2> groundRange() const;
	std::array<int, 2> groundBegan() const { return m_ground_began; }

	double speed = 1;
	bool running = true;

private:
	int32_t m_x, m_y, m_z;
	std::vector<int32_t> m_blocks; // by cell (x fastest, then z, then y), or -1
	const std::vector<sandbox::StageBlock> &m_table;
	std::vector<physics::Actor::Cue> m_cues;
	v3d m_start;
	double m_end;
	std::unique_ptr<physics::Actor> m_actor;
	std::unique_ptr<physics::Driven> m_driven;
	std::optional<physics::Assembly> m_assembly;
	Json::Value m_looks;
	std::vector<physics::Driven::Cue> m_drives;
	std::vector<float> m_heights; // by cell: how tall its block's cut (1: whole)
	struct Loose
	{
		v3s16 lo;  // its first node
		int nodes_x, nodes_z, deep; // sixteenths deep, flat, as built
		const sandbox::StageBlock *block;
		std::unique_ptr<pile::Field> field;
		std::vector<std::array<int, 2>> holes; // columns to cut: x0 z0 x1 z1 d, in pairs below
		float behind = 0;
		std::map<int, v3f> last; // each presser's last press
		u64 version = 0;
	};
	std::optional<Loose> m_loose;
	Json::Value m_ground_json;
	std::array<int, 2> m_ground_began{};
	void makeGround();
	void pressAndSlide(float dt);
	void press(int who, v3f at, float radius, float kpa);
	void boxes(v3s16 n, std::vector<aabb3f> &out) const;
	v3f toScene(v3f at) const;
	int32_t at(v3s16 n) const; // the block there, or -1
};

} // namespace shell
