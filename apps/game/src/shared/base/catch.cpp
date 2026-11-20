// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/shared/base/catch.h"

#include "src/shared/platform/system/log.h"

namespace Catch {
	std::ostream& cout() {
		return rawstream;
	}
	std::ostream& clog() {
		return rawstream;
	}
	std::ostream& cerr() {
		return rawstream;
	}
}
