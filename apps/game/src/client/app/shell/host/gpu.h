// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes.h"
#include <memory>
#include <unordered_map>

namespace shell {

/*
	The graphics card's memory, as NVIDIA's driver says through NVML
	(libnvidia-ml, loaded at run time, never linked): the card's total and free,
	and what each process on it uses. Read off the main thread; no GL needed.
	Other cards (and no NVIDIA driver) have none: open() is null, and the Memory
	tab says the card isn't read.
*/
class Gpu
{
public:
	// The library loaded and the first card found; null when there's none.
	static std::unique_ptr<Gpu> open();
	~Gpu();
	Gpu(const Gpu &) = delete;
	Gpu &operator=(const Gpu &) = delete;

	struct Reading {
		u64 total, free; // bytes
		std::unordered_map<long, u64> processes; // pid → bytes on the card
	};
	// Now. A failed read throws (the card went away under us).
	Reading read();

private:
	struct Impl;
	explicit Gpu(std::unique_ptr<Impl> impl);
	std::unique_ptr<Impl> m;
};

} // namespace shell
