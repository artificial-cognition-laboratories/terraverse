// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/host/console.h"
#include "src/shared/platform/log.h"
#include <chrono>

namespace shell {

// Enough to scroll back through a session's start; the page keeps its own ring.
constexpr size_t KEEP = 5000;

static const char *levelName(LogLevel level)
{
	switch (level) {
	case LL_ERROR: return "error";
	case LL_WARNING: return "warning";
	case LL_ACTION: return "action";
	case LL_NONE: // raw output: what's printed whatever the level
	case LL_INFO: return "info";
	default: return "verbose";
	}
}

Console::Console()
{
	g_logger.addOutputMaxLevel(this, LL_INFO);
}

Console::~Console()
{
	g_logger.removeOutput(this);
}

void Console::logRaw(LogLevel level, std::string_view line)
{
	keep(level, "", line);
}

void Console::log(LogLevel level, const std::string &combined, const std::string &time,
		const std::string &thread_name, std::string_view payload_text)
{
	keep(level, thread_name, payload_text);
}

void Console::keep(LogLevel level, std::string thread, std::string_view text)
{
	// a stream's line arrives with its newline
	while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
		text.remove_suffix(1);
	const s64 now = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
	std::lock_guard<std::mutex> lock(m_lock);
	m_lines.push_back({ ++m_seq, now, level, std::move(thread), std::string(text) });
	if (m_lines.size() > KEEP)
		m_lines.pop_front();
}

Json::Value Console::since(u64 seq, u64 &last) const
{
	Json::Value lines(Json::arrayValue);
	std::lock_guard<std::mutex> lock(m_lock);
	last = seq;
	for (const Line &line : m_lines) {
		if (line.seq <= seq)
			continue;
		Json::Value out;
		out["seq"] = (Json::UInt64)line.seq;
		out["time"] = (Json::Int64)line.time;
		out["level"] = levelName(line.level);
		out["thread"] = line.thread;
		out["text"] = line.text;
		lines.append(std::move(out));
		last = line.seq;
	}
	return lines;
}

} // namespace shell
