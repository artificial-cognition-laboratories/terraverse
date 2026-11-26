// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/shared/platform/online/host.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/platform/system/log.h"
#include "src/shared/platform/system/porting.h"
#include <algorithm>

namespace online {

// A round of STUN requests: a few tries 400 ms apart, then the next round
// after a pause. Routers forget an idle UDP mapping after 30 s or more.
static const int TRIES = 4;
static const u64 RETRY_MS = 400, ROUND_MS = 20000;
// A joiner's client keeps trying for longer than this; punch until it's in.
static const u64 PUNCH_MS = 8000, PUNCH_EVERY_MS = 100;
static const char PUNCH[] = "TFPUNCH";

Host::Host(Api api, int handle, u16 port) :
	m_api(std::move(api)), m_handle(handle), m_port(port), m_caller(httpfetch_caller_alloc_secure())
{
	m_endpoint.lan = lanAddresses(port);
	m_worker = std::thread([this] { work(); });
}

Host::~Host()
{
	m_worker.join();
	httpfetch_caller_free(m_caller);
}

// Slow and blocking: DNS for the STUN servers, then the router.
void Host::work()
{
	std::vector<Address> servers = stunServers();
	{
		std::lock_guard<std::mutex> lock(m_lock);
		for (const Address &server : servers)
			m_asks.push_back({ server, newId(), std::nullopt });
		m_resolved = true;
	}
	std::unique_ptr<PortMapping> mapping = PortMapping::open(m_port);
	std::lock_guard<std::mutex> lock(m_lock);
	m_mapping = std::move(mapping);
	m_upnp_tried = true;
	settle();
}

void Host::stray(const Address &sender, const u8 *data, size_t size)
{
	std::lock_guard<std::mutex> lock(m_lock);
	for (Ask &a : m_asks) {
		std::optional<Address> seen = mappedAddress(data, size, a.id);
		if (seen) {
			a.seen = seen;
			settle();
			return;
		}
	}
}

// What the answers so far say. Under m_lock.
void Host::settle()
{
	Endpoint e;
	e.lan = m_endpoint.lan;
	for (const Ask &a : m_asks) {
		if (!a.seen)
			continue;
		if (!e.pub)
			e.pub = a.seen;
		else if (a.seen->getPort() != e.pub->getPort())
			e.nat = "symmetric";
	}
	if (e.pub && e.nat != "symmetric") {
		bool own = false; // the public address is one of ours: nothing in between
		for (const Address &a : e.lan)
			own = own || a.serializeString() == e.pub->serializeString();
		e.nat = own ? "open" : "cone";
	}
	// The router forwards to us: that beats anything a NAT does.
	if (m_mapping && e.nat != "open") {
		e.pub = m_mapping->external();
		e.nat = "upnp";
	}

	bool answered = true;
	for (const Ask &a : m_asks)
		answered = answered && a.seen;
	// Worth saying once every server has answered, or a round has given up.
	bool known = m_known || answered || m_round >= TRIES || (m_resolved && m_asks.empty()) || m_mapping;
	if (!known)
		return;
	if (!m_known || toJson(e) != toJson(m_endpoint)) {
		m_endpoint = e;
		m_known = true;
		m_changed = true;
		infostream << "online: hosting at " << toJson(e).toStyledString() << std::endl;
	}
}

std::optional<Endpoint> Host::changed()
{
	std::lock_guard<std::mutex> lock(m_lock);
	if (!m_changed)
		return std::nullopt;
	m_changed = false;
	return m_endpoint;
}

// STUN requests from the server's socket. Under m_lock.
void Host::ask(u64 now)
{
	if (!m_resolved || m_asks.empty() || now < m_next_ask)
		return;
	bool answered = true;
	for (const Ask &a : m_asks)
		answered = answered && a.seen;
	if (m_round >= TRIES || (answered && m_round > 0)) {
		// The round is over: say what's known, then start the next one later,
		// with fresh ids (which also keeps the mapping alive).
		settle();
		for (Ask &a : m_asks)
			a.id = newId(); // keep `seen`: a lost round changes nothing
		m_round = 0;
		m_next_ask = now + ROUND_MS;
		return;
	}
	for (const Ask &a : m_asks) {
		std::string req = bindingRequest(a.id);
		sendRaw(m_handle, a.server, req.data(), req.size());
	}
	m_round++;
	m_next_ask = now + RETRY_MS;
}

void Host::punch(const Endpoint &joiner)
{
	const u64 until = porting::getTimeMs() + PUNCH_MS;
	if (joiner.pub)
		m_punches.push_back({ *joiner.pub, until });
	for (const Address &a : joiner.lan)
		m_punches.push_back({ a, until });
	m_next_punch = 0;
}

void Host::step()
{
	const u64 now = porting::getTimeMs();
	{
		std::lock_guard<std::mutex> lock(m_lock);
		ask(now);
	}

	HTTPFetchResult result;
	while (httpfetch_async_get(m_caller, result)) {
		m_waiting = false;
		if (!result.succeeded || result.response_code != 200) {
			warningstream << "online: asking ArcLabs for joiners failed ("
					<< (result.succeeded ? reason(result.response_code, result.data) : "unreachable")
					<< "); asking again shortly" << std::endl;
			m_next_knocks = now + 5000;
			continue;
		}
		for (const Json::Value &knock : parse(result.data)["knocks"]) {
			infostream << "online: " << knock["username"].asString() << " is joining" << std::endl;
			punch(endpointFromJson(knock["endpoint"]));
		}
	}

	// Wait on ArcLabs for the next joiner: it answers as soon as one knocks.
	if (!m_waiting && now >= m_next_knocks) {
		HTTPFetchRequest req = request(m_api, HTTP_GET, "/api/terrafirma/knocks?wait=20", Json::Value(), 30000);
		req.caller = m_caller;
		httpfetch_async(req);
		m_waiting = true;
	}

	if (!m_punches.empty() && now >= m_next_punch) {
		m_punches.erase(std::remove_if(m_punches.begin(), m_punches.end(),
				[now](const Punch &p) { return now > p.until; }), m_punches.end());
		for (const Punch &p : m_punches)
			sendRaw(m_handle, p.to, PUNCH, sizeof(PUNCH) - 1);
		m_next_punch = now + PUNCH_EVERY_MS;
	}
}

} // namespace online
