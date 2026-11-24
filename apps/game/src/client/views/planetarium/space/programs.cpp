// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/planetarium/space/programs.h"

namespace planetarium {

Programs::Programs(IShaderSource *source) :
	lit(source),
	rim(source),
	galaxy(source)
{}

} // namespace planetarium
