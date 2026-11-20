# The kit every pure library's tests share: doctest's runner, the notes stream
# and library_test(), which a library calls from its own CMakeLists for the
# tests in its tests/ folder. One small binary each, linking only its library,
# so a change to terragen rebuilds terragen and its tests, never the engine.
# Each source file is its own ctest, labelled `hot` (its quick cases) or `slow`
# (cases in doctest's "slow" suite): `ctest -L hot -j` is the quick loop.

add_library(library_tests STATIC note.cpp)
add_custom_target(all_library_tests) # every library's test binary: apps/game/tests/run/library.sh builds it
target_include_directories(library_tests PUBLIC "${GAME_DIR}")

# library_test(<name> MAIN <main.cpp> LINK <targets...> SOURCES <files...>)
function(library_test name)
	cmake_parse_arguments(T "" "MAIN" "LINK;SOURCES" ${ARGN})
	if(NOT T_MAIN)
		set(T_MAIN "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/main.cpp")
	endif()
	add_executable(${name}_tests ${T_MAIN} ${T_SOURCES})
	target_link_libraries(${name}_tests PRIVATE library_tests ${T_LINK})
	add_dependencies(all_library_tests ${name}_tests)
	set_target_properties(${name}_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${REPO_DIR}/.output/tests")
	foreach(src ${T_SOURCES})
		get_filename_component(file ${src} NAME)
		get_filename_component(stem ${src} NAME_WE)
		add_test(NAME ${name}.${stem} COMMAND ${name}_tests --source-file=*${file} --test-suite-exclude=slow)
		set_tests_properties(${name}.${stem} PROPERTIES LABELS "hot;${name}")
		file(READ ${src} text)
		string(FIND "${text}" "test_suite(\"slow\")" has_slow)
		if(has_slow GREATER -1)
			add_test(NAME ${name}.${stem}.slow COMMAND ${name}_tests --source-file=*${file} --test-suite=slow)
			set_tests_properties(${name}.${stem}.slow PROPERTIES LABELS "slow;${name}")
		endif()
	endforeach()
endfunction()
