// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/platform/log_internal.h"
#include <json/json.h>
#include <deque>
#include <mutex>

namespace shell {

/*
	The engine's log, kept for the debugger's Console: the last lines, up to
	info, from every thread. Lines are numbered as they arrive so the page can
	be sent only what it hasn't had.
*/
class Console : public ILogOutput
{
public:
	Console();
	~Console();
	Console(const Console &) = delete;
	Console &operator=(const Console &) = delete;

	void logRaw(LogLevel level, std::string_view line) override;
	void log(LogLevel level, const std::string &combined, const std::string &time,
			const std::string &thread_name, std::string_view payload_text) override;

	// The lines kept after `seq`, oldest first, as the page's LogLine[]; and the last one's number.
	Json::Value since(u64 seq, u64 &last) const;

private:
	struct Line
	{
		u64 seq;
		s64 time; // ms since the epoch
		LogLevel level;
		std::string thread;
		std::string text;
	};
	void keep(LogLevel level, std::string thread, std::string_view text);

	mutable std::mutex m_lock;
	std::deque<Line> m_lines;
	u64 m_seq = 0;
};

} // namespace shell
