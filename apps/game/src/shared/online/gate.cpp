// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/shared/online/gate.h"
#include "src/shared/core/exceptions.h"
#include "src/shared/platform/log.h"

namespace online {

Gate::Gate(Opts opts) : m_opts(std::move(opts)), m_caller(httpfetch_caller_alloc_secure())
{
}

Gate::~Gate()
{
	httpfetch_caller_free(m_caller);
}

void Gate::begin(u16 peer, const std::string &name, const std::string &ticket, const std::string &build)
{
	if (build != m_opts.build) {
		m_ready.push_back({ peer, false, "This world runs Terra Firma build " + m_opts.build +
				"; you have " + build + ". You both need the same build to play together." });
		return;
	}
	if (name == m_opts.owner) {
		// Only the host's own game knows the secret.
		bool own = !ticket.empty() && ticket == m_opts.secret;
		m_ready.push_back({ peer, own, own ? "" : "That's the host's name." });
		return;
	}

	Json::Value body;
	body["ticket"] = ticket;
	HTTPFetchRequest req = request(m_opts.api, HTTP_POST, "/api/terrafirma/tickets/verify", body);
	req.caller = m_caller;
	req.request_id = ++m_next_request;
	m_pending[req.request_id] = { peer, name };
	httpfetch_async(req);
}

std::vector<Gate::Decision> Gate::poll()
{
	HTTPFetchResult result;
	while (httpfetch_async_get(m_caller, result)) {
		auto it = m_pending.find(result.request_id);
		if (it == m_pending.end())
			continue;
		Pending pending = it->second;
		m_pending.erase(it);

		if (!result.succeeded || result.response_code >= 500) {
			warningstream << "online: couldn't check " << pending.name << "'s ticket ("
					<< result.response_code << ")" << std::endl;
			m_ready.push_back({ pending.peer, false, "The host couldn't check your ticket with ArcLabs. Try again." });
			continue;
		}
		if (result.response_code != 200) {
			m_ready.push_back({ pending.peer, false, reason(result.response_code, result.data) });
			continue;
		}
		Json::Value player = parse(result.data)["player"];
		if (!player["username"].isString())
			throw BaseException("online: tickets/verify answered without a player");
		// The ticket is someone's: it must be the name this client claims.
		const std::string owner = player["username"].asString();
		bool admit = owner == pending.name;
		if (!admit)
			actionstream << "online: " << pending.name << " presented " << owner << "'s ticket" << std::endl;
		m_ready.push_back({ pending.peer, admit, admit ? "" : "That ticket belongs to someone else." });
	}
	std::vector<Decision> out;
	out.swap(m_ready);
	return out;
}

} // namespace online
