// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/host/framework.h"
#include "src/shared/base/exceptions.h"
#include <include/internal/cef_types.h>

#ifdef __APPLE__
	#include <include/wrapper/cef_library_loader.h>
	#include <include/internal/cef_string.h>
	#include <mach-o/dyld.h>
	#include <climits>
	#include <cstdlib>
	#include <sys/stat.h>
#endif

namespace shell {

#ifdef __APPLE__

namespace {
std::string frameworksDir();
bool exists(const std::string &path);
} // namespace

// The helper's name is the bundle's: Chromium finds its (GPU), (Renderer), … variants
// as siblings, named by suffix (shell/CMakeLists.txt builds them all).
constexpr const char *HELPER = "Terraverse Helper";
constexpr const char *FRAMEWORK = "Chromium Embedded Framework.framework";

struct Framework::Impl
{
	std::string dir = frameworksDir();
	CefScopedLibraryLoader loader;
};

Framework::Framework() : m(std::make_unique<Impl>())
{
	const std::string lib = m->dir + "/" + FRAMEWORK + "/Chromium Embedded Framework";
	if (!exists(lib))
		throw BaseException("shell: Chromium's framework isn't at " + lib + " (build the cef_runtime target)");
	if (!m->loader.LoadInMain())
		throw BaseException("shell: Chromium's framework at " + lib + " wouldn't load");
}

void Framework::place(_cef_settings_t &settings) const
{
	CefString(&settings.framework_dir_path) = m->dir + "/" + FRAMEWORK;
	CefString(&settings.browser_subprocess_path) =
			m->dir + "/" + HELPER + ".app/Contents/MacOS/" + HELPER;
}

namespace {

// <the executable's folder>/../Frameworks, resolved: the place LoadInMain reads.
std::string frameworksDir()
{
	char path[PATH_MAX];
	uint32_t size = sizeof(path);
	if (_NSGetExecutablePath(path, &size) != 0)
		throw BaseException("shell: the executable's path is longer than PATH_MAX");
	char real[PATH_MAX];
	if (!realpath(path, real))
		throw BaseException(std::string("shell: can't resolve the executable's path ") + path);
	std::string exe(real);
	return exe.substr(0, exe.rfind('/')) + "/../Frameworks";
}

bool exists(const std::string &path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0;
}

} // namespace

#else

struct Framework::Impl
{
};

Framework::Framework() : m(std::make_unique<Impl>())
{
}

void Framework::place(_cef_settings_t &) const
{
}

#endif

Framework::~Framework() = default;

} // namespace shell
