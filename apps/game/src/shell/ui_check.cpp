// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "ui_check.h"
#include "log.h"
#include "porting.h"
#include "trace/trace.h"
#include <IEventReceiver.h>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace shell {

namespace {
constexpr u64 SECOND = 1'000'000'000;
constexpr u64 SETTLE = 3 * SECOND; // the page and the planet behind it come in
constexpr u64 SWEEP = 8 * SECOND;  // the mouse at work
constexpr u32 SCROLL_EVERY = 20;   // moves between turns of the wheel

// What a frame may cost the UI, and how slow the page may be to answer (ms).
// From the rework of sprint 2026-11-20: the main thread's UI was 1-37 ms a
// frame, input to screen 2-4 frames with 150 ms tails.
struct Budget
{
	const char *what;
	double most; // ms
};
constexpr Budget UI_P99 = { "main thread's UI, 99th percentile", 1.0 };
constexpr Budget UI_WORST = { "main thread's UI, worst frame", 8.0 };
constexpr Budget INPUT_P95 = { "input to screen, 95th percentile", 34.0 }; // two frames
constexpr Budget INPUT_WORST = { "input to screen, worst", 100.0 };
constexpr Budget GAP_WORST = { "page's longest frame gap", 70.0 };
constexpr u32 LEAST_ANSWERS = 20; // fewer, and the page wasn't answering the mouse at all

double percentile(std::vector<double> v, double p)
{
	if (v.empty())
		return 0;
	std::sort(v.begin(), v.end());
	return v[std::min(v.size() - 1, (size_t)(p * (v.size() - 1) + 0.5))];
}
} // namespace

struct UiCheck::Stats
{
	std::vector<double> ui;    // ms a frame
	std::vector<double> input; // ms, each answered input
	double gap = 0;            // ms
	double held = 0;           // frames
	u32 frames = 0;
};

UiCheck::UiCheck(Opts opts) : m_opts(std::move(opts)), m_stats(std::make_unique<Stats>()) {}
UiCheck::~UiCheck() = default;

void UiCheck::step(core::dimension2du screen)
{
	if (m_done)
		return;
	const u64 now = trace::now();
	if (!m_ready_at) {
		if (m_opts.ready())
			m_ready_at = now;
		return;
	}
	if (now - m_ready_at < SETTLE) {
		for (const trace::Frame &f : trace::framesSince(m_last_frame)) // (not counted: settling)
			m_last_frame = f.id;
		return;
	}

	// what the frames since cost
	static const u32 UI = trace::intern("UI"), INPUT = trace::intern("UI: input to screen"),
			GAP = trace::intern("UI: page frame gap"), HELD = trace::intern("UI: page frames held [#]");
	for (const trace::Frame &f : trace::framesSince(m_last_frame)) {
		m_last_frame = f.id;
		m_stats->frames++;
		double ui = 0;
		for (const trace::Frame::Thread &t : f.threads)
			if (t.thread == "Main")
				for (const auto &[name, took] : t.parts)
					if (name == UI)
						ui = took / 1e6;
		m_stats->ui.push_back(ui);
		for (const auto &[name, value] : f.counters) {
			if (name == INPUT)
				m_stats->input.push_back(value / 1000);
			else if (name == GAP)
				m_stats->gap = std::max(m_stats->gap, value / 1000);
			else if (name == HELD)
				m_stats->held += value;
		}
	}

	if (now - m_ready_at < SETTLE + SWEEP) {
		// a Lissajous figure over the page, crossing its buttons and rows
		const double t = (now - m_ready_at - SETTLE) / 1e9;
		SEvent e{};
		e.EventType = EET_MOUSE_INPUT_EVENT;
		e.MouseInput.X = (s32)(screen.Width * (0.5 + 0.45 * std::sin(t * 2.7)));
		e.MouseInput.Y = (s32)(screen.Height * (0.5 + 0.45 * std::sin(t * 3.7 + 1)));
		e.MouseInput.Event = EMIE_MOUSE_MOVED;
		m_opts.input(e);
		if (++m_moves % SCROLL_EVERY == 0) {
			e.MouseInput.Event = EMIE_MOUSE_WHEEL;
			e.MouseInput.Wheel = (m_moves / SCROLL_EVERY) % 2 ? -1.f : 1.f;
			m_opts.input(e);
		}
		return;
	}

	// the verdict
	m_done = true;
	bool pass = true;
	const auto judge = [&](const Budget &b, double value) {
		const bool ok = value <= b.most;
		pass &= ok;
		actionstream << "UICHECK " << b.what << ": " << value << " ms (budget " << b.most << ")"
				<< (ok ? "" : " OVER") << std::endl;
	};
	const Stats &s = *m_stats;
	actionstream << "UICHECK " << s.frames << " frames, " << m_moves << " moves, "
			<< s.input.size() << " answered, " << s.held << " page frames held" << std::endl;
	judge(UI_P99, percentile(s.ui, 0.99));
	judge(UI_WORST, s.ui.empty() ? 0 : *std::max_element(s.ui.begin(), s.ui.end()));
	judge(INPUT_P95, percentile(s.input, 0.95));
	judge(INPUT_WORST, s.input.empty() ? 0 : *std::max_element(s.input.begin(), s.input.end()));
	judge(GAP_WORST, s.gap);
	if (s.input.size() < LEAST_ANSWERS) {
		actionstream << "UICHECK the page answered " << s.input.size() << " inputs (fewer than "
				<< LEAST_ANSWERS << "): the sweep never reached it" << std::endl;
		pass = false;
	}
	actionstream << (pass ? "UICHECK pass" : "UICHECK fail") << std::endl;
	*porting::signal_handler_killstatus() = true; // (the menu's loop ends on it)
}

} // namespace shell
