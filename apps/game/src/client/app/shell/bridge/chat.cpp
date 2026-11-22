// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/bridge/chat.h"
#include "src/client/app/shell/bridge/bridge.h"
#include "src/client/app/shell/host/shell.h"
#include "src/client/world/session/client.h"
#include "src/client/world/objects/localplayer.h"
#include "src/shared/game/chat/chatmessage.h"
#include "src/shared/platform/settings/settings.h"
#include "src/shared/util/text/string.h"
#include "src/shared/base/exceptions.h"

namespace shell {

static const char *const VERBS[] = { "chat.send", "chat.players" };

namespace {

// the server's kinds of line, as the page names them
const char *toneOf(ChatMessageType type);
const char *levelOf(LogLevel level);
LogLevel chatLogLevel();

} // namespace

Chat::Chat(Opts o) : m_shell(o.shell), m_client(o.client), m_log(g_logger)
{
	m_log.setLogLevel(chatLogLevel());
	m_shell.on("chat.send", [this](const Json::Value &intent) {
		const Json::Value &text = intent["text"];
		if (!text.isString() || trim(text.asString()).empty())
			throw Refused("chat.send needs a line of 'text'");
		if (text.asString().find_first_of("\r\n") != std::string::npos)
			throw Refused("a chat line is one line");
		m_client->typeChatMessage(utf8_to_wide(trim(text.asString())));
		return Json::Value();
	});
	m_shell.on("chat.players", [this](const Json::Value &) {
		// (everyone else: nobody's told something by themselves)
		const std::string me = m_client->getEnv().getLocalPlayer()->getName();
		Json::Value names(Json::arrayValue);
		for (const std::string &name : m_client->getConnectedPlayerNames())
			if (name != me)
				names.append(name);
		return names;
	});
}

Chat::~Chat()
{
	for (const char *verb : VERBS)
		m_shell.off(verb);
}

void Chat::update()
{
	while (std::optional<ChatMessage> said = m_client->takeChatMessage()) {
		Json::Value line;
		line["from"] = said->sender.empty() ? Json::Value() : Json::Value(wide_to_utf8(said->sender));
		line["text"] = wide_to_utf8(unescape_enriched(said->message));
		line["tone"] = toneOf(said->type);
		Json::Value message;
		message["kind"] = "chat";
		message["line"] = line;
		m_shell.send(message);
	}
	for (const LogEntry &entry : m_log.take()) {
		Json::Value message;
		message["kind"] = "log";
		message["level"] = levelOf(entry.level);
		message["text"] = entry.combined;
		m_shell.send(message);
	}
}

namespace {

const char *toneOf(ChatMessageType type)
{
	switch (type) {
	case CHATMESSAGE_TYPE_NORMAL: return "said";
	case CHATMESSAGE_TYPE_ANNOUNCE: return "announce";
	default: return "server"; // a reply, or the server speaking
	}
}

const char *levelOf(LogLevel level)
{
	switch (level) {
	case LL_ERROR: return "error";
	case LL_WARNING: return "warning";
	case LL_ACTION: return "action";
	default: return "info";
	}
}

// The chat_log_level setting: what the log shows the page (the menu's Developer log).
LogLevel chatLogLevel()
{
	const LogLevel level = Logger::stringToLevel(g_settings->get("chat_log_level"));
	if (level == LL_MAX)
		throw BaseException("chat_log_level is " + g_settings->get("chat_log_level") +
				", not a log level (none, error, warning, action, info, verbose, trace)");
	return level;
}

} // namespace

} // namespace shell
