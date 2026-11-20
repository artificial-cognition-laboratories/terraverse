// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "bridge.h"
#include "convert_json.h"
#include "exceptions.h"
#include "log.h"
#include "trace/trace.h"
#include <json/json.h>
#include <sstream>

namespace shell {

// Failure codes the page sees (ui/src/bridge/types.ts: FailureCode).
enum Failure { MALFORMED = 1, UNKNOWN_INTENT = 2, REFUSED = 3, ENGINE_ERROR = 4 };

void Bridge::on(const std::string &kind, Verb verb)
{
	if (!m_verbs.emplace(kind, std::move(verb)).second)
		throw BaseException("shell: intent '" + kind + "' already has a handler");
}

void Bridge::onAsync(const std::string &kind, Verb verb)
{
	on(kind, std::move(verb));
	m_async.insert(kind);
	if (!m_worker.joinable())
		m_worker = std::thread([this] { work(); });
}

void Bridge::off(const std::string &kind)
{
	if (m_verbs.erase(kind) == 0)
		throw BaseException("shell: intent '" + kind + "' had no handler to remove");
	m_async.erase(kind);
}

void Bridge::drain()
{
	std::unique_lock<std::mutex> guard(m_lock);
	m_idle.wait(guard, [this] { return m_stopping || (m_jobs.empty() && !m_working); });
}

void Bridge::stop()
{
	{
		std::lock_guard<std::mutex> guard(m_lock);
		m_stopping = true;
		m_jobs.clear();
	}
	m_wake.notify_all();
	m_idle.notify_all();
	if (m_worker.joinable())
		m_worker.join();
}

// (a query's callback answers from any thread of the browser process)
static void deliver(CefRefPtr<CefMessageRouterBrowserSide::Callback> callback, const std::string &kind,
		bool ok, int code, const std::string &text)
{
	if (ok) {
		callback->Success(text);
		return;
	}
	if (code == REFUSED)
		infostream << "shell: " << kind << " refused: " << text << std::endl;
	else
		errorstream << "shell: " << kind << " failed: " << text << std::endl;
	callback->Failure(code, text);
}

void Bridge::work()
{
	for (;;) {
		Job job;
		{
			std::unique_lock<std::mutex> guard(m_lock);
			m_wake.wait(guard, [this] { return m_stopping || !m_jobs.empty(); });
			if (m_stopping)
				return;
			job = std::move(m_jobs.front());
			m_jobs.pop_front();
			m_working = true;
		}
		// (at the end of this round however it ends)
		struct Done
		{
			Bridge &bridge;
			~Done()
			{
				std::lock_guard<std::mutex> guard(bridge.m_lock);
				bridge.m_working = false;
				bridge.m_idle.notify_all();
			}
		} done{ *this };
		try {
			deliver(job.callback, job.kind, true, 0, fastWriteJson(job.verb(job.intent)));
		} catch (const Refused &e) {
			deliver(job.callback, job.kind, false, REFUSED, e.what());
		} catch (const std::exception &e) {
			deliver(job.callback, job.kind, false, ENGINE_ERROR, e.what());
		}
	}
}

bool Bridge::OnQuery(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		int64_t query_id, const CefString &request, bool persistent,
		CefRefPtr<Callback> callback)
{
	m_inbox.post([this, request = request.ToString(), callback] { answer(request, callback); });
	return true;
}

void Bridge::answer(const std::string &request, CefRefPtr<Callback> callback)
{
	Json::Value intent;
	std::string errors;
	std::istringstream in(request);
	if (!Json::parseFromStream(Json::CharReaderBuilder(), in, &intent, &errors) ||
			!intent.isObject() || !intent["kind"].isString()) {
		errorstream << "shell: malformed intent: " << request << std::endl;
		callback->Failure(MALFORMED, "malformed intent: " + errors);
		return;
	}

	std::string kind = intent["kind"].asString();
	auto verb = m_verbs.find(kind);
	if (verb == m_verbs.end()) {
		errorstream << "shell: no handler for intent " << kind << std::endl;
		callback->Failure(UNKNOWN_INTENT, "nothing handles '" + kind + "' right now");
		return;
	}

	verbosestream << "shell: intent " << kind << std::endl;
	// (each verb its own zone under "UI: inbox": a slow one names itself)
	const trace::Zone zone(trace::intern("UI: " + kind));
	// A verb runs in the game's frame: one slower than half a frame says so
	// in the log (it belongs on onAsync, or needs making cheaper)
	struct Slow
	{
		const std::string &kind;
		const u64 start = trace::now();
		~Slow()
		{
			const double ms = (trace::now() - start) / 1e6;
			if (ms > 8)
				warningstream << "shell: intent " << kind << " took " << ms << " ms of the frame" << std::endl;
		}
	} slow{ kind };
	if (m_async.count(kind)) {
		{
			std::lock_guard<std::mutex> guard(m_lock);
			m_jobs.push_back({ kind, verb->second, intent, callback });
		}
		m_wake.notify_one();
		return;
	}
	try {
		callback->Success(fastWriteJson(verb->second(intent)));
	} catch (const Refused &e) {
		infostream << "shell: " << kind << " refused: " << e.what() << std::endl;
		callback->Failure(REFUSED, e.what());
	} catch (const std::exception &e) {
		errorstream << "shell: " << kind << " failed: " << e.what() << std::endl;
		callback->Failure(ENGINE_ERROR, e.what());
	}
}

void Bridge::send(CefRefPtr<CefBrowser> browser, const Json::Value &message)
{
	// JSON is a valid JS expression, so the message is passed as a literal.
	std::string js = "window.__terra.receive(" + fastWriteJson(message) + ")";
	browser->GetMainFrame()->ExecuteJavaScript(js, "terra://bridge", 0);
}

} // namespace shell
