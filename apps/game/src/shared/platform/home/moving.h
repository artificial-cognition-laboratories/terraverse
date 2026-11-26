// Terraverse: moving what an older layout left into the home's (.agents/planning/home.md,
// "Moving in"): once, the first time a root has no profiles/.
#pragma once

#include <string>

namespace home::moving {

// What the root (and a source build's `legacy` folder, if any) held the old way, moved into
// `local`: worlds, account, per-player files to the profile, settings to settings.conf, the
// rest to the cache. Leaves MOVED.md where things were taken from. Throws home::Error.
void in(const std::string &root, const std::string &legacy);

} // namespace home::moving
