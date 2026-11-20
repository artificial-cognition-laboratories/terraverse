// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/host/scheme.h"
#include "src/shared/platform/system/filesys.h"
#include "src/shared/platform/system/log.h"
#include <include/cef_parser.h>
#include <include/wrapper/cef_stream_resource_handler.h>

namespace shell {

static CefRefPtr<CefResourceHandler> notFound(const std::string &url)
{
	errorstream << "shell: no UI file for " << url << std::endl;
	static const char body[] = "not found";
	return new CefStreamResourceHandler(404, "Not Found", "text/plain", {},
			CefStreamReader::CreateForData((void *)body, sizeof(body) - 1));
}

CefRefPtr<CefResourceHandler> UiScheme::Create(CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame, const CefString &scheme_name,
		CefRefPtr<CefRequest> request)
{
	std::string url = request->GetURL();
	CefURLParts parts;
	if (!CefParseURL(url, parts))
		return notFound(url);

	std::string path = CefString(&parts.path);
	if (path.empty() || path == "/")
		path = "/index.html";
	if (path.find("..") != std::string::npos)
		return notFound(url);

	std::string file = m_root + path;
	if (!fs::PathExists(file) || fs::IsDir(file))
		return notFound(url);

	size_t dot = path.rfind('.');
	std::string mime = dot == std::string::npos ? "" : CefGetMimeType(path.substr(dot + 1)).ToString();
	if (mime.empty())
		mime = "application/octet-stream";
	return new CefStreamResourceHandler(mime, CefStreamReader::CreateForFile(file));
}

} // namespace shell
