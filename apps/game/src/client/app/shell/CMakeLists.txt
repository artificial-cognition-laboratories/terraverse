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
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/registry.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/library.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/world_server.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/console.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/cpu_canvas.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/debugger.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge/entering.cpp
	${GAME_DIR}/src/client/views/scenes/kinds/course.cpp
	${GAME_DIR}/src/client/world/sim/fine/finewater.cpp
	${GAME_DIR}/src/client/world/sim/fine/finepowder.cpp
	${GAME_DIR}/src/client/world/sim/fine/plume.cpp
	${GAME_DIR}/src/client/views/scenes/kinds/relief.cpp
	${GAME_DIR}/src/client/views/scenes/kinds/flora.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/host/frame_blur.cpp
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
set(shell_SRCS ${shell_SRCS} PARENT_SCOPE)

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
