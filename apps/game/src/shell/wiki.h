// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "course.h"
#include "matter.h"
#include "relief.h"
#include "orbit/flight.h"
#include "scene.h"
#include "sandbox/stage.h"
#include <json/json.h>
#include <map>
#include <memory>
#include <mutex>

namespace shell {

class Shell;

/*
	The wiki's verbs, answered for the whole session (in the menu and in
	game alike): the matter tables (MatterVerbs) and the sandboxes its demos
	run in.

	  sandbox.create { size: [x, y, z], ambient (K), pressure (Pa), air: { id: share },
	                   blocks: [{ at, node?, add?: { id: moles }, temperature?, hold? }] } → { id }:
	                   a few blocks of the game's (sandbox::Stage), the world's own matter running in them
	  sandbox.step   { id, seconds } → the stage's view (see view()); off the main thread
	  sandbox.drop   { id }
	  scene.show     { id, rect: { x, y, width, height } (page pixels), heat }:
	                 that sandbox drawn natively there, under the page (Scene)
	  scene.place    { rect }; scene.hide
	  moves.list     { scenes: movement | vehicles } → its scenes, as <kind>.json has them
	  moves.show     { scenes, id, rect, right }: that one played natively (Course), on a loop
	  moves.set      { speed, running }; moves.restart; moves.state → { time, doing, stance, pace }
	  relief.show    { rect, right }; relief.set { lens, speed, running }; relief.restart; relief.state → { round, rounds }:
	                 the Worlds time-lapse (Relief)
	  orbit.coast    { altitude, speed, angle, mass, drag_area, seconds }: Newton's cannonball
*/
class Wiki
{
public:
	explicit Wiki(Shell &shell);
	~Wiki();

private:
	Json::Value create(const Json::Value &intent);
	Json::Value step(const Json::Value &intent);
	Json::Value set(const Json::Value &intent);
	Json::Value coast(const Json::Value &intent);
	sandbox::Stage &find(const Json::Value &intent);
	Json::Value view(const sandbox::Stage &s) const;
	void publish(int id, const sandbox::Stage &s); // what the scene shows next, if it's showing that one
	core::recti rectOf(const Json::Value &rect) const;

	Shell &m_shell;
	MatterVerbs m_matter;
	orbit::Flight m_flight{ terragen::home() };
	std::mutex m_lock; // over m_sandboxes and each sandbox (stepping runs off the main thread)
	std::map<int, std::unique_ptr<sandbox::Stage>> m_stages;
	std::vector<sandbox::StageBlock> m_blocks; // the game's, as baked: read when first needed
	const std::vector<sandbox::StageBlock> &blocks();
	int m_next = 1;
	Scene m_scene; // the main thread's
	std::unique_ptr<Course> m_course; // the main thread's; while one plays no sandbox is shown
	u64 m_course_at = 0;
	std::unique_ptr<Relief> m_relief; // the same
	Json::Value m_river;
	const Json::Value &moves(const std::string &kind);
	std::map<std::string, Json::Value> m_moves; // by kind: movement, vehicles
	Json::Value m_machines; // the palette's baked
	int m_shown = 0; // the sandbox it shows (0: none)
	std::mutex m_published; // stepping's latest, for the scene: never held while stepping
	std::optional<Scene::Frame> m_latest;
	int m_latestId = 0;
};

} // namespace shell
