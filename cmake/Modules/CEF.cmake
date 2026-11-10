# Prebuilt Chromium Embedded Framework for the web shell (src/shell).
# Downloaded once into the build tree and verified against a pinned hash.

set(CEF_VERSION "156.0.7+g47dbc1d+chromium-156.0.8078.17")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(CEF_PLATFORM "linux64")
	set(CEF_SHA1 "c8c8055f9ad040a42691be6f04fb6107e2309c8d")
elseif(WIN32)
	set(CEF_PLATFORM "windows64")
	set(CEF_SHA1 "e13cb97c5be44784c2dba6df960791ea5cf6d406")
else()
	message(FATAL_ERROR "ENABLE_SHELL is wired for Linux and Windows only")
endif()
set(CEF_DIST "cef_binary_${CEF_VERSION}_${CEF_PLATFORM}_minimal")
set(CEF_DEPS_DIR "${CMAKE_BINARY_DIR}/_deps")
set(CEF_ROOT "${CEF_DEPS_DIR}/${CEF_DIST}")

if(NOT IS_DIRECTORY "${CEF_ROOT}/cmake")
	string(REPLACE "+" "%2B" cef_url_dist "${CEF_DIST}")
	message(STATUS "Downloading CEF ${CEF_VERSION}")
	file(DOWNLOAD "https://cef-builds.spotifycdn.com/${cef_url_dist}.tar.bz2"
		"${CEF_DEPS_DIR}/cef.tar.bz2"
		EXPECTED_HASH SHA1=${CEF_SHA1}
		SHOW_PROGRESS)
	file(ARCHIVE_EXTRACT INPUT "${CEF_DEPS_DIR}/cef.tar.bz2" DESTINATION "${CEF_DEPS_DIR}")
	file(REMOVE "${CEF_DEPS_DIR}/cef.tar.bz2")
endif()

# On Windows: the C runtime the engine and vcpkg use (CEF defaults to static),
# and no sandbox, as on Linux.
set(CEF_RUNTIME_LIBRARY_FLAG "/MD" CACHE STRING "" FORCE)
set(USE_SANDBOX OFF CACHE BOOL "" FORCE)

# CEF's bootstrap forces a build type when none is set; ours is always set by now.
list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)

add_subdirectory("${CEF_LIBCEF_DLL_WRAPPER_PATH}" libcef_dll_wrapper EXCLUDE_FROM_ALL)
# The wrapper is linked into an RTTI + exceptions binary, and CEF's -Werror
# trips on newer compilers than it was released against.
if(MSVC)
	target_compile_options(libcef_dll_wrapper PRIVATE /GR /WX-)
else()
	target_compile_options(libcef_dll_wrapper PRIVATE -frtti -Wno-error)
endif()
