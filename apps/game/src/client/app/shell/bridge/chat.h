// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/platform/system/log_internal.h"

class Client;

namespace shell {

class Shell;

/*
	Chat, as the page shows it (the palette's feed), for one session:

	  chat.send    { text }: a line said, or a command (`/…`): the server answers either
	  chat.players → [name]: who else is here, for the palette's @

	and each frame the lines the server sent, one `chat` message each ({ line: { from, text,
	tone } }: `from` who said it, null for the server's own), and what the log says at the
	chat_log_level setting's level or worse, `log` messages the page keeps apart, out of chat.
*/
class Chat
{
public:
	struct Opts
	{
		Shell &shell;
		Client *client;
	};
	explicit Chat(Opts o);
	~Chat();
	Chat(const Chat &) = delete;
	Chat &operator=(const Chat &) = delete;

	// Once a frame: what was said since, and what was logged.
	void update();

private:
	Shell &m_shell;
	Client *m_client;
	CaptureLogOutput m_log;
};

} // namespace shell
