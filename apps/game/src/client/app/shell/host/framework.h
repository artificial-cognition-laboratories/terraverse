// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <memory>
#include <string>

struct _cef_settings_t;

namespace shell {

/*
	Chromium's library, loaded for as long as this lives. On macOS CEF's functions
	exist only once its framework is loaded at run time, from Frameworks/ beside the
	executable's folder (`.output/Frameworks` in place, `Contents/Frameworks` in an app),
	with its helper apps there too; elsewhere it's linked and this does nothing.
	Build it before any CEF call, drop it after CefShutdown.
*/
class Framework
{
public:
	Framework(); // throws if the framework isn't there or won't load
	~Framework();

	// Where Chromium finds its framework and helper apps, written into its settings.
	void place(_cef_settings_t &settings) const;

private:
	struct Impl;
	std::unique_ptr<Impl> m;
};

} // namespace shell
