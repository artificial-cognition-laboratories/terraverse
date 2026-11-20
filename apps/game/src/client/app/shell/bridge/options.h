// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>
#include <string>

namespace shell {

/*
	The engine settings the page may read and change, and the range each is
	allowed. Anything not in the table is not the page's to touch.
*/
class Options
{
public:
	Json::Value get() const;
	// Validates, applies, and saves to the user's config. Throws Refused.
	void set(const std::string &key, const Json::Value &value) const;
	// Validate every value before applying a preset, then save once.
	void apply(const Json::Value &values) const;
	Json::Value bindings() const;
	void bind(const std::string &key, int scancode, bool replace) const;
	void resetBindings() const;
};

} // namespace shell
