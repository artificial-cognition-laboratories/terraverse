// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/shared/platform/online/ref.h"

#include <cctype>
#include <regex>

namespace online::ref {

namespace {
std::string trimmed(const std::string &s);
std::string lower(std::string s);
std::optional<Ref> address(const std::string &rest);
} // namespace

std::optional<Ref> parse(const std::string &text)
{
	static const std::regex code(R"(^TF\d+\.[A-Za-z0-9._-]+$)");
	static const std::regex published(R"(^@([a-z0-9-]+)/([a-z0-9-]+)$)");
	static const std::regex player(R"(^friend/([A-Za-z0-9_.-]{1,32})$)");

	std::string rest = trimmed(text);
	const std::string scheme = SCHEME;
	if (lower(rest.substr(0, scheme.size())) == scheme)
		rest = rest.substr(scheme.size());
	else if (rest.find("://") != std::string::npos)
		return std::nullopt;
	if (!rest.empty() && rest.back() == '/')
		rest.pop_back();
	if (rest.empty())
		return std::nullopt;

	std::smatch m;
	if (std::regex_match(rest, code))
		return Code{rest};
	if (rest[0] == '@') {
		if (!std::regex_match(rest, m, published))
			return std::nullopt;
		return Published{m[1], m[2]};
	}
	// "friend" is a kind, never a host
	if (rest == "friend" || rest.rfind("friend/", 0) == 0) {
		if (!std::regex_match(rest, m, player))
			return std::nullopt;
		return Friend{m[1]};
	}
	return address(rest);
}

std::string print(const Ref &ref)
{
	const std::string scheme = SCHEME;
	if (auto *a = std::get_if<Address>(&ref)) {
		bool v6 = a->host.find(':') != std::string::npos;
		return scheme + (v6 ? "[" + a->host + "]" : a->host) + ":" + std::to_string(a->port);
	}
	if (auto *c = std::get_if<Code>(&ref))
		return scheme + c->code;
	if (auto *p = std::get_if<Published>(&ref))
		return scheme + "@" + p->scope + "/" + p->name;
	return scheme + "friend/" + std::get<Friend>(ref).player;
}

namespace {

std::optional<Ref> address(const std::string &rest)
{
	static const std::regex host(R"(^(?:\[([0-9a-fA-F:.]+)\]|([A-Za-z0-9.-]+))(?::(\d{1,5}))?$)");
	std::smatch m;
	if (!std::regex_match(rest, m, host))
		return std::nullopt;
	long port = m[3].matched ? std::stol(m[3]) : DEFAULT_PORT;
	if (port < 1 || port > 65535)
		return std::nullopt;
	if (m[2].matched) {
		const std::string name = m[2];
		if (name.front() == '.' || name.back() == '.' || name.front() == '-')
			return std::nullopt;
		return Address{lower(name), (uint16_t)port};
	}
	return Address{m[1], (uint16_t)port};
}

std::string trimmed(const std::string &s)
{
	size_t a = s.find_first_not_of(" \t\r\n");
	if (a == std::string::npos)
		return "";
	size_t b = s.find_last_not_of(" \t\r\n");
	return s.substr(a, b - a + 1);
}

std::string lower(std::string s)
{
	for (char &c : s)
		c = (char)std::tolower((unsigned char)c);
	return s;
}

} // namespace

} // namespace online::ref
