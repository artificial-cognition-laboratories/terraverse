// What mods ask of the server: each call read from its frame, put to the Host, and its answer.
// The sidecar sends what comes back; a call it can't read is a bug in terra (mp::Error).
#pragma once

#include "src/server/app/mods/sidecar/sidecar.h"
#include <functional>

namespace sidecar {

namespace mp { class Value; class Writer; }

class Calls {
public:
	struct Opts {
		Host &host;
	};
	explicit Calls(Opts o);

	// [call, ...args] → the answer's writer. Throws mp::Error for a call it can't read.
	std::function<void(mp::Writer &)> answer(const mp::Value &message);

private:
	Host &m_host;
};

} // namespace sidecar
