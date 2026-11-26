// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/render/gputime/gputime.h"
#include "library/trace/trace.h"
#include <IVideoDriver.h>
#include <mt_opengl.h>
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace gpu {

using GLuint = unsigned int; // as mt_opengl.h types them (privately)
using GLuint64 = std::uint64_t;

namespace {
// frames in flight: a frame's queries are read this many frames on
constexpr size_t RING = 4;
Clock::Impl *g_clock = nullptr; // the installed clock (install)
} // namespace

// A part's names as counters, interned once
struct Names
{
	std::string gpu, draws, triangles;
};

struct Clock::Impl
{
	video::IVideoDriver &driver;
	const bool timed; // timestamp queries there

	// A part as timed in one frame: its two timestamps' queries
	struct Part
	{
		const Names *names;
		GLuint begin, end;
		u32 draws0, triangles0;
	};
	struct Frame
	{
		std::vector<Part> parts;
		std::vector<GLuint> spare;  // queries made, free for reuse
		GLuint start = 0, edge = 0; // the frame's begin() and end
		bool issued = false;
		bool started = false; // begin() this frame
	};
	std::array<Frame, RING> ring;
	size_t now = 0;
	std::unordered_map<std::string, Names> names;

	GLuint query(Frame &f)
	{
		if (!f.spare.empty()) {
			GLuint q = f.spare.back();
			f.spare.pop_back();
			return q;
		}
		GLuint q = 0;
		GL.GenQueries(1, &q);
		return q;
	}

	GLuint stamp(Frame &f)
	{
		GLuint q = query(f);
		GL.QueryCounter(q, GL.TIMESTAMP);
		return q;
	}

	const Names &named(std::string_view name)
	{
		auto it = names.find(std::string(name));
		if (it == names.end()) {
			const std::string n(name);
			it = names.emplace(n, Names{ "GPU: " + n + " [us]", "Draws: " + n + " [#]", "Triangles: " + n + " [#]" }).first;
		}
		return it->second;
	}

	// a frame issued RING frames ago, its queries long done: reported, and freed
	void read(Frame &f)
	{
		if (!f.issued)
			return;
		const auto at = [](GLuint q) {
			GLuint64 t = 0;
			GL.GetQueryObjectui64v(q, GL.QUERY_RESULT, &t);
			return t;
		};
		for (const Part &p : f.parts) {
			if (p.begin && p.end) {
				trace::count(p.names->gpu, (double)(at(p.end) - at(p.begin)) / 1000.0, trace::Fold::Sum);
				f.spare.push_back(p.begin);
				f.spare.push_back(p.end);
			}
		}
		if (f.started)
			trace::count("GPU: frame [us]", (double)(at(f.edge) - at(f.start)) / 1000.0, trace::Fold::Last);
		f.started = false;
		f.parts.clear();
		f.issued = false;
	}
};

Clock::Clock(Opts o) :
	m(std::make_unique<Impl>(Impl{ o.driver, GL.QueryCounter != nullptr && GL.GetQueryObjectui64v != nullptr }))
{}

Clock::~Clock()
{
	if (g_clock == m.get())
		g_clock = nullptr;
	if (!m->timed)
		return;
	for (Impl::Frame &f : m->ring) {
		for (const Impl::Part &p : f.parts)
			for (GLuint q : { p.begin, p.end })
				if (q)
					GL.DeleteQueries(1, &q);
		for (GLuint q : f.spare)
			GL.DeleteQueries(1, &q);
		for (GLuint q : { f.start, f.edge })
			if (q)
				GL.DeleteQueries(1, &q);
	}
}

void Clock::begin()
{
	if (!m->timed)
		return;
	Impl::Frame &f = m->ring[m->now];
	if (!f.start)
		GL.GenQueries(1, &f.start);
	GL.QueryCounter(f.start, GL.TIMESTAMP);
	f.started = true;
}

void Clock::frame()
{
	const video::SFrameStats &stats = m->driver.getFrameStats();
	trace::count("Draws: frame [#]", stats.Drawcalls, trace::Fold::Last);
	trace::count("Triangles: frame [#]", stats.PrimitivesDrawn, trace::Fold::Last);
	if (!m->timed)
		return;
	Impl::Frame &f = m->ring[m->now];
	// the frame's end (the next one's start)
	if (!f.edge)
		GL.GenQueries(1, &f.edge);
	GL.QueryCounter(f.edge, GL.TIMESTAMP);
	f.issued = true;
	// the oldest frame, about to be reused: its queries are done by now
	m->now = (m->now + 1) % RING;
	m->read(m->ring[m->now]);
}

void install(Clock *clock)
{
	g_clock = clock ? clock->m.get() : nullptr;
}

Zone::Zone(std::string_view name) noexcept :
	m_clock(g_clock), m_slot(-1)
{
	if (!m_clock)
		return;
	Clock::Impl &c = *m_clock;
	Clock::Impl::Frame &f = c.ring[c.now];
	const video::SFrameStats &stats = c.driver.getFrameStats();
	f.parts.push_back({ &c.named(name), c.timed ? c.stamp(f) : 0, 0, stats.Drawcalls, stats.PrimitivesDrawn });
	m_slot = (int)f.parts.size() - 1;
}

Zone::~Zone() noexcept
{
	if (!m_clock || m_slot < 0)
		return;
	Clock::Impl &c = *m_clock;
	Clock::Impl::Frame &f = c.ring[c.now];
	Clock::Impl::Part &p = f.parts[m_slot];
	if (c.timed)
		p.end = c.stamp(f);
	const video::SFrameStats &stats = c.driver.getFrameStats();
	trace::count(p.names->draws, stats.Drawcalls - p.draws0, trace::Fold::Sum);
	trace::count(p.names->triangles, stats.PrimitivesDrawn - p.triangles0, trace::Fold::Sum);
}

} // namespace gpu
