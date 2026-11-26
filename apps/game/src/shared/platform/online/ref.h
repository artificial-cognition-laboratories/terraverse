// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

/*
	terra:// refs: every place you can go, one address. The same grammar as terra's
	(apps/cli/src/terra/ref), both held to one golden table (assets/misc/refs.json).
	A ref never carries a secret: a server on this machine admits by its running/ record.
*/
namespace online::ref {

constexpr const char *SCHEME = "terra://";
// Where a server listens when its ref doesn't say.
constexpr uint16_t DEFAULT_PORT = 30000;

// A server at an address.
struct Address { std::string host; uint16_t port; };
// A clean universe from a world code, opened alone.
struct Code { std::string code; };
// A published server, by its registry name.
struct Published { std::string scope; std::string name; };
// A friend's open world.
struct Friend { std::string player; };

using Ref = std::variant<Address, Code, Published, Friend>;

// What text refers to, or nothing when it isn't a ref.
std::optional<Ref> parse(const std::string &text);
// Its one spelling.
std::string print(const Ref &ref);

} // namespace online::ref
