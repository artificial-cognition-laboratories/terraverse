// terra, the CLI beside the engine: found once, then run to completion for a job
// (installing a world's mods). The sidecar is terra too, but long-lived: sidecar.h.
#pragma once

#include <string>
#include <vector>

namespace sidecar {

class Terra {
public:
	struct Ran {
		int code;            // its exit code; -1 if it didn't exit by itself
		std::string output;  // stdout and stderr, together
	};

	// TERRA_CLI (the terra that started this engine, from source under terral), else the
	// one installed beside it. Throws Error if there's none.
	static Terra locate();

	const std::string &path() const { return m_path; }

	// `terra <args>` in `cwd`, waited for. Throws Error only if it can't be started.
	Ran run(const std::vector<std::string> &args, const std::string &cwd) const;

private:
	explicit Terra(std::string path) : m_path(std::move(path)) {}
	std::string m_path;
};

} // namespace sidecar
