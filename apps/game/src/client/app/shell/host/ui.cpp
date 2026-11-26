// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/host/ui.h"
#include "src/client/app/shell/bridge/bridge.h"
#include "src/client/app/shell/host/shell.h"
#include "src/client/world/session/client.h"
#include "src/shared/base/exceptions.h"
#include <sstream>

namespace shell {

void Ui::update()
{
	for (const auto &[key, text] : m_opts.client->takeUiValues()) {
		Json::Value value;
		std::string errors;
		std::istringstream in(text);
		// the server wrote this with jsoncpp: unreadable means a bug, not input
		if (!Json::parseFromStream(Json::CharReaderBuilder(), in, &value, &errors))
			throw BaseException("shell: the server sent UI value '" + key + "' that isn't JSON: " + errors);
		if (value.isNull())
			m_values.erase(key);
		else
			m_values[key] = value;

		post(key, value);
	}
}

void Ui::replay()
{
	for (const auto &[key, value] : m_values)
		post(key, value);
}

void Ui::post(const std::string &key, const Json::Value &value)
{
	Json::Value message;
	message["kind"] = "ui";
	message["key"] = key;
	message["value"] = value;
	m_opts.shell.send(message);
}

const Json::Value &Ui::get(const std::string &key) const
{
	static const Json::Value null;
	auto it = m_values.find(key);
	return it == m_values.end() ? null : it->second;
}

void Ui::send(const Json::Value &intent)
{
	if (!intent["name"].isString())
		throw Refused("ui.send needs a string 'name'");
	Json::StreamWriterBuilder writer;
	writer["indentation"] = "";
	m_opts.client->sendUiEvent(intent["name"].asString(), Json::writeString(writer, intent["payload"]));
}

} // namespace shell
