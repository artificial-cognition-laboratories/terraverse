// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes.h"
#include <json/json.h>
#include <memory>
#include <string>
#include <vector>

namespace shell {

/*
	Where the memory goes, for the debugger's Memory tab, sampled on a thread of
	its own from the session's start: ten times a second the cheap figures (the
	heap, the owners, the graphics card), once a second the dear ones (each
	process's smaps takes milliseconds; between, a sample carries the last).
	Five minutes kept at ten a second for the page, an hour at one a second for
	the report. Each sample is:

	- every process of the game's tree: the engine, Chromium's (GPU, renderers,
	  utilities), terra's sidecar; each one's resident, proportional, private and
	  swapped memory (smaps_rollup);
	- the machine: memory available, swap used;
	- the engine's heap, as malloc counts it;
	- the graphics card's memory, total and free, and each process's on it
	  (NVIDIA's driver says, gpu.h; other cards aren't read yet);
	- each owner's own figure (library/trace/owned.h), and what no owner claims.

	Linux only for now: elsewhere a sample says so (`supported` false).
*/
class Memory
{
public:
	struct Opts {
		std::string dir; // where reports are written: the user's traces/
	};

	struct Process {
		long pid;
		std::string role; // "engine", "chromium gpu-process", "terra runtime", …
		u64 rss, pss, priv, swap; // bytes
		u64 gpu; // on the graphics card, bytes (0 when it's none, or unknown)
	};
	struct Owner {
		std::string name;
		u64 bytes, count;
		bool gpu; // on the graphics card
	};
	struct Sample {
		u64 seq;
		double t; // ms since the engine started (trace::now)
		std::vector<Process> processes;
		u64 available, total;     // the machine's memory, bytes
		u64 swap_used, swap_total;
		u64 heap; // in use, as malloc counts it (arenas and mapped chunks)
		u64 gpu_total, gpu_free; // the graphics card's, bytes; 0 when its driver doesn't say
		u64 gpu_others; // on the card, by processes outside the game's tree
		std::vector<Owner> owners;
	};

	explicit Memory(Opts opts);
	~Memory();
	Memory(const Memory &) = delete;
	Memory &operator=(const Memory &) = delete;

	// The samples after `seq`, as the page takes them, and the newest's seq.
	struct Since {
		Json::Value samples;
		u64 newest;
	};
	Since since(u64 seq) const;

	// Every kept sample written as a report folder (summary.md, samples.jsonl):
	// its path. Throws when it can't be written.
	std::string report() const;

private:
	struct Impl;
	std::unique_ptr<Impl> m;
};

namespace memory {

// One sample as JSON, as the page and the report's lines read it.
Json::Value json(const Memory::Sample &sample);
// The report folder for `samples`, under `dir`: its path.
std::string write(const std::string &dir, const std::vector<Memory::Sample> &samples);

} // namespace memory

} // namespace shell
