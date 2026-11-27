set(shell_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/account.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/actions.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/app.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/bridge.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/client.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/community.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/local_mods.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/local_servers.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/profiles.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/identity.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/registry.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/library.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/world_server.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/console.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/cpu_canvas.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/debugger.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/inspectors.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/entering.cpp
	${GAME_DIR}/src/client/views/scenes/kinds/course.cpp
	${GAME_DIR}/src/client/world/sim/fine/finewater.cpp
	${GAME_DIR}/src/client/world/sim/fine/finepowder.cpp
	${GAME_DIR}/src/client/world/sim/fine/plume.cpp
	${GAME_DIR}/src/client/views/scenes/kinds/relief.cpp
	${GAME_DIR}/src/client/views/scenes/kinds/flora.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/frame_blur.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/framework.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/gl_restore.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/ui_scale.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/gpu_canvas.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/gpu_egl.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/gpu_programs.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/ime.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/inbox.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/input.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/items.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/latency.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/menu.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/options.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/pixels.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/process.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/pump.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../../views/preview/preview.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/scheme.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/serverinfo.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/screens.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/session.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/loading.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../../views/scenes/kinds/planetarium.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/shell.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/social.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/state.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/ui.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/ui_check.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/system.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/matter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/wiki.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/chat.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/figure.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/scene_rect.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/underlays.cpp
	${GAME_DIR}/src/client/views/scenes/scene.cpp
	${GAME_DIR}/src/client/views/figure/figure_mesh.cpp
	${GAME_DIR}/src/client/views/figure/figure_view.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/surface.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/worlds.cpp
)
if(APPLE)
	# Chromium's protocol on SDL's NSApplication (host/app_protocol.h): Objective-C++
	list(APPEND shell_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/host/app_protocol.mm)
endif()
set(shell_SRCS ${shell_SRCS} PARENT_SCOPE)

# macOS: Chromium's framework and its helper apps in Frameworks/ beside the
# executable's folder (.output/Frameworks in place; Contents/Frameworks in an app),
# where CefScopedLibraryLoader looks (host/framework.h). A helper is its own small
# executable, copied into each variant Chromium asks for by name.
function(add_cef_runtime_macos dir)
	get_filename_component(frameworks "${dir}/../Frameworks" ABSOLUTE)
	set(framework "Chromium Embedded Framework.framework")
	set(stamp "${CMAKE_CURRENT_BINARY_DIR}/cef_framework.stamp")
	add_custom_command(
		OUTPUT "${stamp}"
		COMMAND ${CMAKE_COMMAND} -E rm -rf "${frameworks}/${framework}"
		COMMAND ${CMAKE_COMMAND} -E make_directory "${frameworks}"
		COMMAND cp -R "${CEF_BINARY_DIR_RELEASE}/${framework}" "${frameworks}/"
		COMMAND ${CMAKE_COMMAND} -E touch "${stamp}"
		DEPENDS "${CEF_BINARY_DIR_RELEASE}/${framework}/Chromium Embedded Framework"
		COMMENT "Copying Chromium's framework into ${frameworks}"
		VERBATIM)
	set(cef_runtime_outputs "${stamp}")

	set(shell_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
	add_executable(shell_helper "${shell_dir}/host/helper_mac.cpp" "${shell_dir}/host/app.cpp"
		"${shell_dir}/host/pump.cpp")
	target_compile_features(shell_helper PRIVATE cxx_std_20)
	target_include_directories(shell_helper PRIVATE "${GAME_DIR}")
	target_include_directories(shell_helper SYSTEM PRIVATE "${CEF_ROOT}")
	target_link_libraries(shell_helper libcef_dll_wrapper)
	set_target_properties(shell_helper PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shell_helper")

	# "<name suffix>:<target suffix>:<plist suffix>", CEF's list of the variants
	foreach(spec IN LISTS CEF_HELPER_APP_SUFFIXES)
		string(REGEX MATCH "^([^:]*):([^:]*):(.*)$" _ "${spec}")
		set(HELPER_NAME "Terraverse Helper${CMAKE_MATCH_1}")
		set(HELPER_ID "net.terraverse.helper${CMAKE_MATCH_3}")
		set(plist "${CMAKE_CURRENT_BINARY_DIR}/helper${CMAKE_MATCH_3}.plist")
		configure_file("${shell_dir}/host/helper-Info.plist.in" "${plist}")
		set(app "${frameworks}/${HELPER_NAME}.app/Contents")
		add_custom_command(
			OUTPUT "${app}/MacOS/${HELPER_NAME}" "${app}/Info.plist"
			COMMAND ${CMAKE_COMMAND} -E make_directory "${app}/MacOS"
			COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:shell_helper>" "${app}/MacOS/${HELPER_NAME}"
			COMMAND ${CMAKE_COMMAND} -E copy "${plist}" "${app}/Info.plist"
			DEPENDS shell_helper "${plist}"
			VERBATIM)
		list(APPEND cef_runtime_outputs "${app}/MacOS/${HELPER_NAME}")
	endforeach()

	add_custom_target(cef_runtime DEPENDS ${cef_runtime_outputs})
endfunction()

# Chromium's runtime files next to the executable. libcef.so ships with debug
# info (~1.5 GB); the copy is stripped. libcef.dll keeps its symbols apart.
function(add_cef_runtime dir)
	if(WIN32)
		set(cef_runtime_outputs "")
	else()
		set(cef_runtime_outputs "${dir}/libcef.so")
		add_custom_command(
			OUTPUT "${dir}/libcef.so"
			COMMAND ${CMAKE_COMMAND} -E make_directory "${dir}"
			COMMAND ${CMAKE_STRIP} --strip-unneeded -o "${dir}/libcef.so" "${CEF_BINARY_DIR}/libcef.so"
			DEPENDS "${CEF_BINARY_DIR}/libcef.so"
			COMMENT "Stripping libcef.so into ${dir}")
	endif()

	foreach(file IN LISTS CEF_BINARY_FILES)
		if(file STREQUAL "libcef.so" OR file STREQUAL "chrome-sandbox")
			continue()
		endif()
		list(APPEND cef_runtime_outputs "${dir}/${file}")
		add_custom_command(
			OUTPUT "${dir}/${file}"
			COMMAND ${CMAKE_COMMAND} -E make_directory "${dir}"
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CEF_BINARY_DIR}/${file}" "${dir}/${file}"
			DEPENDS "${CEF_BINARY_DIR}/${file}")
	endforeach()

	foreach(file IN LISTS CEF_RESOURCE_FILES)
		list(APPEND cef_runtime_outputs "${dir}/${file}")
		if(IS_DIRECTORY "${CEF_RESOURCE_DIR}/${file}")
			add_custom_command(
				OUTPUT "${dir}/${file}"
				COMMAND ${CMAKE_COMMAND} -E copy_directory "${CEF_RESOURCE_DIR}/${file}" "${dir}/${file}"
				DEPENDS "${CEF_RESOURCE_DIR}/${file}")
		else()
			add_custom_command(
				OUTPUT "${dir}/${file}"
				COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CEF_RESOURCE_DIR}/${file}" "${dir}/${file}"
				DEPENDS "${CEF_RESOURCE_DIR}/${file}")
		endif()
	endforeach()

	add_custom_target(cef_runtime DEPENDS ${cef_runtime_outputs})
endfunction()
