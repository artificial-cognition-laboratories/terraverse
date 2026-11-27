// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/host/gpu.h"
#include "src/shared/base/exceptions.h"
#include <string>
#include <vector>

#if defined(__linux__)
	#include <dlfcn.h>
#endif

namespace shell {

#if defined(__linux__)

namespace {

// NVML's own shapes (nvml.h), as much as is read: its header isn't vendored.
using Device = void *;
struct NvMemory {
	unsigned long long total, free, used;
};
struct NvProcess {
	unsigned int pid;
	unsigned long long used;
	unsigned int gpuInstance, computeInstance;
};
constexpr int OK = 0;
constexpr int TOO_SMALL = 7; // NVML_ERROR_INSUFFICIENT_SIZE
constexpr unsigned long long UNKNOWN = ~0ULL; // NVML_VALUE_NOT_AVAILABLE

} // namespace

struct Gpu::Impl
{
	void *lib;
	Device device;
	int (*memory)(Device, NvMemory *);
	int (*graphics)(Device, unsigned int *, NvProcess *);
	int (*shutdown)();
};

std::unique_ptr<Gpu> Gpu::open()
{
	void *lib = dlopen("libnvidia-ml.so.1", RTLD_NOW | RTLD_LOCAL);
	if (!lib)
		return nullptr; // no NVIDIA driver: not a failure, a card we don't read
	auto init = (int (*)())dlsym(lib, "nvmlInit_v2");
	auto byIndex = (int (*)(unsigned int, Device *))dlsym(lib, "nvmlDeviceGetHandleByIndex_v2");
	auto impl = std::make_unique<Impl>(Impl{ lib, nullptr,
			(int (*)(Device, NvMemory *))dlsym(lib, "nvmlDeviceGetMemoryInfo"),
			(int (*)(Device, unsigned int *, NvProcess *))dlsym(lib, "nvmlDeviceGetGraphicsRunningProcesses_v3"),
			(int (*)())dlsym(lib, "nvmlShutdown") });
	if (!init || !byIndex || !impl->memory || !impl->graphics || !impl->shutdown) {
		dlclose(lib);
		return nullptr; // a driver too old for what's read
	}
	if (init() != OK) {
		dlclose(lib);
		return nullptr;
	}
	if (byIndex(0, &impl->device) != OK) {
		impl->shutdown();
		dlclose(lib);
		return nullptr;
	}
	return std::unique_ptr<Gpu>(new Gpu(std::move(impl)));
}

Gpu::Gpu(std::unique_ptr<Impl> impl) : m(std::move(impl))
{
}

Gpu::~Gpu()
{
	m->shutdown();
	dlclose(m->lib);
}

Gpu::Reading Gpu::read()
{
	NvMemory memory{};
	if (m->memory(m->device, &memory) != OK)
		throw BaseException("gpu: NVML couldn't read the card's memory");
	Reading out{ memory.total, memory.free, {} };
	std::vector<NvProcess> list(16);
	unsigned int count = (unsigned int)list.size();
	int said = m->graphics(m->device, &count, list.data());
	if (said == TOO_SMALL) {
		list.resize(count + 8);
		count = (unsigned int)list.size();
		said = m->graphics(m->device, &count, list.data());
	}
	if (said != OK)
		throw BaseException("gpu: NVML couldn't list the card's processes (" + std::to_string(said) + ")");
	for (unsigned int i = 0; i < count; i++)
		if (list[i].used != UNKNOWN)
			out.processes[(long)list[i].pid] += list[i].used;
	return out;
}

#else

// TEMP(2026-11-27): NVML is read on Linux only so far (nvml.dll on Windows is the way in)
struct Gpu::Impl {};
std::unique_ptr<Gpu> Gpu::open() { return nullptr; }
Gpu::Gpu(std::unique_ptr<Impl> impl) : m(std::move(impl)) {}
Gpu::~Gpu() = default;
Gpu::Reading Gpu::read() { return {}; }

#endif

} // namespace shell
