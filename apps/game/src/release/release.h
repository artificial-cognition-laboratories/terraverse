// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace release {

// The key CI signs manifests with. gate.sh checks it against scripts/release/update-key.pub.
extern const char *const UPDATE_KEY;

// A release version ("v0.1.1" or "0.1.1") as numbers. Nothing for a development
// build, whose id is a git hash.
std::optional<std::vector<int>> parse(const std::string &version);

// Does the signature (base64) cover exactly these bytes, under this PEM public key?
bool verify(const std::string &bytes, const std::string &signature, const std::string &key);

// The version a signed manifest names. Nothing if it can't be trusted.
std::optional<std::string> latest(const std::string &manifest, const std::string &signature, const std::string &key);

// Before a server opens a world: refuses one saved by a newer release (throws
// BaseException, for the player to read), copies one saved by another release
// into backups/, and records this build as the last to save it. Development
// builds change nothing, so working on the game never stamps a world.
void openWorld(const std::string &world, const std::string &build, const std::string &backups);

} // namespace release
