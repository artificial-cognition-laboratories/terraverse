// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <include/cef_scheme.h>
#include <string>

namespace shell {

// not shell://: that's the public address of a place (online/ref.h)
constexpr const char *SCHEME = "shell";
constexpr const char *UI_HOST = "ui";
constexpr const char *ITEM_HOST = "item";
// a local mod's pictures, as terra drew them for its Contents (bridge/local_mods.h)
constexpr const char *CONTENTS_HOST = "contents";

// Serves a directory as shell://<host>/…: the built UI (ui/dist) as
// shell://ui/, giving the page a real origin so ES modules load (Chromium
// refuses them over file://), and rendered item icons as shell://item/.
class UiScheme : public CefSchemeHandlerFactory
{
public:
	explicit UiScheme(const std::string &root) : m_root(root) {}

	CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
			CefRefPtr<CefFrame> frame, const CefString &scheme_name,
			CefRefPtr<CefRequest> request) override;

private:
	std::string m_root;

	IMPLEMENT_REFCOUNTING(UiScheme);
};

} // namespace shell
