// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

// Chromium's helper processes on macOS (renderer, GPU, …): their own executable in
// their own app, as macOS needs, where Linux and Windows run the engine again
// (main.cpp, shell::runHelper). It loads the framework from three folders up.
#include "src/client/app/shell/host/app.h"
#include <include/wrapper/cef_library_loader.h>

int main(int argc, char *argv[])
{
	CefScopedLibraryLoader loader;
	if (!loader.LoadInHelper())
		return 1;
	CefMainArgs args(argc, argv);
	CefRefPtr<shell::App> app = new shell::App();
	return CefExecuteProcess(args, app, nullptr);
}
