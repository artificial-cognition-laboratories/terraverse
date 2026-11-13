// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "shell/entering.h"
#include "shell/planetarium.h"
#include "shell/shell.h"
#include "porting.h"
#include <IVideoDriver.h>
#include <ICursorControl.h>
#include <algorithm>
#include <cmath>

namespace shell {

namespace {

// The stages, in order: the load screen's progress each spans (from upstream's
// calls: 0 loading, 5 the server, 10–20 connecting, 25–65 definitions and
// media, 66–100 textures, shaders and nodes), whether that progress counts
// within it, and roughly how long it takes (seconds, a local world), for the
// whole bar.
struct Stage
{
	const char *id;
	const char *label;
	int from, to;
	bool counts;
	double seconds;
};

const Stage STAGES[] = {
	{ "start", "Starting up", 0, 5, false, 0.3 },
	{ "server", "Opening the world", 5, 10, false, 2.5 },
	{ "connect", "Joining it", 10, 25, false, 0.4 },
	{ "content", "Receiving blocks and media", 25, 66, true, 0.8 },
	{ "prepare", "Preparing textures and shaders", 66, 101, true, 2.0 },
};
constexpr size_t N = sizeof STAGES / sizeof STAGES[0];

double now()
{
	return porting::getTimeMs() / 1000.0;
}

} // namespace

void Entering::begin()
{
	m_active = true;
	m_seen.assign(N, Seen{});
	m_at = 0;
	m_share.reset();
	m_told = Json::Value();
	m_last = now();
	stage(0, std::nullopt);
}

void Entering::end()
{
	if (!m_active)
		return;
	m_active = false;
	m_opts.shell.planetarium().release();
	Json::Value message;
	message["kind"] = "entering";
	message["loading"] = Json::Value();
	m_opts.shell.send(message);
	m_told = Json::Value();
}

void Entering::step(int percent)
{
	size_t at = 0;
	for (size_t k = 0; k < N; k++)
		if (percent >= STAGES[k].from)
			at = k;
	const Stage &s = STAGES[at];
	stage(at, s.counts ? std::optional<double>(std::clamp((double)(percent - s.from) / (s.to - 1 - s.from), 0.0, 1.0))
			: std::nullopt);
	frame();
}

void Entering::stage(size_t index, std::optional<double> share)
{
	const double t = now();
	// the load screen's progress only goes forward: an earlier stage's call
	// (upstream draws "Loading..." again, say) is part of the one running
	if (index >= m_at) {
		if (index > m_at)
			m_seen[m_at].until = t;
		m_at = index;
		m_share = share;
	}
	if (!m_seen[m_at].begun) {
		m_seen[m_at].begun = true;
		m_seen[m_at].since = t;
	}
	m_seen[m_at].until = t;
	tell();
}

void Entering::frame()
{
	const double t = now();
	video::IVideoDriver *driver = m_opts.driver;
	Planetarium &planet = m_opts.shell.planetarium();
	driver->beginScene(true, true, video::SColor(255, 12, 14, 12));
	planet.pointer(m_opts.cursor->getPosition());
	planet.tick(std::min(t - m_last, 0.25)); // (a long step, a blocking one, isn't a jump)
	m_last = t;
	planet.draw(driver);
	m_opts.shell.frame();
	driver->endScene();
}

// The page is told at once when a stage starts, else at most ten times a second
void Entering::tell()
{
	Json::Value stages(Json::arrayValue);
	double total = 0, behind = 0;
	for (size_t k = 0; k < N; k++) {
		const Stage &stage = STAGES[k];
		const Seen &seen = m_seen[k];
		if (k < m_at && !seen.begun)
			continue; // not this way in (a friend's world opens no server here)
		total += stage.seconds;
		Json::Value out;
		out["id"] = stage.id;
		out["label"] = stage.label;
		out["seconds"] = seen.begun ? std::round((seen.until - seen.since) * 10) / 10 : 0;
		out["share"] = Json::Value();
		if (k < m_at) {
			out["state"] = "done";
			behind += stage.seconds;
		} else if (k == m_at) {
			out["state"] = "running";
			if (m_share) {
				out["share"] = std::round(*m_share * 100) / 100;
				behind += stage.seconds * *m_share;
			}
		} else {
			out["state"] = "waiting";
		}
		stages.append(out);
	}
	Json::Value loading;
	loading["share"] = std::round(behind / total * 100) / 100;
	loading["stages"] = stages;
	if (loading == m_told)
		return;
	const auto running = [](const Json::Value &l) {
		for (const Json::Value &s : l["stages"])
			if (s["state"] == "running")
				return s["id"].asString();
		return std::string();
	};
	const double t = now();
	if (!m_told.isNull() && running(loading) == running(m_told) && t - m_toldAt < 0.1)
		return;
	m_told = loading;
	m_toldAt = t;
	Json::Value message;
	message["kind"] = "entering";
	message["loading"] = loading;
	m_opts.shell.send(message);
}

} // namespace shell
