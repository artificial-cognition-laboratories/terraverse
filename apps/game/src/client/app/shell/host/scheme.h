// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <include/cef_scheme.h>
#include <string>

namespace shell {

constexpr const char *SCHEME = "terra";
constexpr const char *UI_HOST = "ui";
constexpr const char *ITEM_HOST = "item";

// Serves a directory as terra://<host>/…: the built UI (ui/dist) as
// terra://ui/, giving the page a real origin so ES modules load (Chromium
// refuses them over file://), and rendered item icons as terra://item/.
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
