// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 Luanti core developers & community

#pragma once

#include "src/server/app/server.h"

class MockServer : public Server
{
public:
	MockServer(const std::string &path_world = "fakepath") :
		Server(path_world, SubgameSpec("fakespec", "fakespec"),
			   true, UDPSocket::CreateEphemeral(false), true, nullptr
		)
	{}

	void start() = delete;
	void stop() = delete;

private:
	void SendChatMessage(session_t peer_id, const ChatMessage &message) override {}
};
