// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace online {

/*
	A world the game opened, listed for this machine while it's open: a run secret its gate
	admits by (gate.h, `local`), and a record in the home's running/ beside terra's servers
	(apps/cli: src/terra/running), marked `"by": "game"`. Whatever on this machine can read the
	record can join: the user's own tools, and through terra, axond's agents (sprint 127).
	The record is the user's alone (0600, in a 0700 folder), and goes with this.
*/
class Listed
{
public:
	struct Opts {
		std::string dir;   // the home's running/
		uint16_t port;     // the server's
		std::string title; // the world's name, as the Play list shows it
		std::string world; // its folder
		std::string owner; // the account playing it: its admin
	};

	// The record written. Throws when it can't be.
	static std::unique_ptr<Listed> open(Opts opts);
	// The record removed, if it's still this process's.
	~Listed();
	Listed(const Listed &) = delete;
	Listed &operator=(const Listed &) = delete;

	// What the gate admits by: 32 random bytes, hex.
	const std::string &secret() const { return m_secret; }
	uint16_t port() const { return m_port; }

private:
	Listed(std::string file, std::string secret, uint16_t port);

	const std::string m_file;
	const uint16_t m_port;
	const std::string m_secret;
};

} // namespace online
