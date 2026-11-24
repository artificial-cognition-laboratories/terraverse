# terragen: the world model. No engine dependencies — the map generator, the
# menu preview and its own tests (tests/) all link it as a
# static library, so a change here rebuilds terragen and relinks, never more.
set(terragen_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/src/column/amplify.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/column/landform/catalogue.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/column/landform/landform.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/out/atlas.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/system/body.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/system/calendar.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/grid/climate.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/surface/craters.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/grid/drainage.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/grid/erosion.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/code/code.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/base/fixed.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/grid/geology.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/grid/grid.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/grid/grid_file.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/grid/history.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/grid/coast.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/grid/crust.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/grid/drift.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/grid/fertility.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/grid/past.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/grid/setting.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/grid/water.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/grid/wear.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/surface/maria.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/surface/lid.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/rivers/streams.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/rivers/streams_grow.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/rivers/river_line.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/base/noise.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/base/parallel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/system/planet.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/out/preview.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/base/progress.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/column/soil.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/base/sphere.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/system/sun.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/system/system.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/column/vegetation.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/world.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/code/worlds.cpp)

# What a grid is built from: every source but those only a column's read runs
# (the drainage land, soil, cover, the preview, the atlas). A kept grid is
# named by their hash, so a change to how columns are read keeps the planets
# already worked out (grid_file.cpp); anything else here works them out again.
set(terragen_sampling streams.cpp streams_grow.cpp river_line.cpp soil.cpp vegetation.cpp preview.cpp atlas.cpp progress.cpp)
file(GLOB_RECURSE terragen_headers "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
set(terragen_grid_SRCS ${terragen_headers})
foreach(src ${terragen_SRCS})
	get_filename_component(name ${src} NAME)
	if(NOT name IN_LIST terragen_sampling)
		list(APPEND terragen_grid_SRCS ${src})
	endif()
endforeach()

# The hash of what terragen's grids are built from: kept grids are named by it
# (sorted: a list's order mustn't change the hash)
list(SORT terragen_grid_SRCS)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/terragen_grid_sources.txt" "${terragen_grid_SRCS}") # (a list in -D would split the command)
add_custom_command(
	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/terragen_grid_sources.cpp"
	COMMAND ${CMAKE_COMMAND}
	-D "GRID_SRCS_FILE=${CMAKE_CURRENT_BINARY_DIR}/terragen_grid_sources.txt"
	-D "TEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/src/grid_sources.cpp.in"
	-D "OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/terragen_grid_sources.cpp"
	-P "${GAME_DIR}/cmake/Modules/HashGridSources.cmake"
	DEPENDS
		${terragen_grid_SRCS}
		"${CMAKE_CURRENT_SOURCE_DIR}/src/grid_sources.cpp.in"
		"${GAME_DIR}/cmake/Modules/HashGridSources.cmake")
# And of all terragen: what a world model's kept river levels and atlas
# layouts are named by (anything outside it may change and keep them)
file(GLOB_RECURSE terragen_all_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
list(SORT terragen_all_SRCS)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/terragen_sources.txt" "${terragen_all_SRCS}")
add_custom_command(
	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/terragen_sources.cpp"
	COMMAND ${CMAKE_COMMAND}
	-D "GRID_SRCS_FILE=${CMAKE_CURRENT_BINARY_DIR}/terragen_sources.txt"
	-D "TEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/src/terragen_sources.cpp.in"
	-D "OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/terragen_sources.cpp"
	-P "${GAME_DIR}/cmake/Modules/HashGridSources.cmake"
	DEPENDS
		${terragen_all_SRCS}
		"${CMAKE_CURRENT_SOURCE_DIR}/src/terragen_sources.cpp.in"
		"${GAME_DIR}/cmake/Modules/HashGridSources.cmake")

find_package(Threads REQUIRED)
add_library(terragen STATIC
	${terragen_SRCS}
	"${CMAKE_CURRENT_BINARY_DIR}/terragen_grid_sources.cpp"
	"${CMAKE_CURRENT_BINARY_DIR}/terragen_sources.cpp")
target_include_directories(terragen PUBLIC "${GAME_DIR}")
target_link_libraries(terragen PUBLIC Threads::Threads)

if(BUILD_UNITTESTS)
	library_test(terragen
		MAIN ${CMAKE_CURRENT_SOURCE_DIR}/tests/main.cpp
		LINK terragen
		SOURCES
			${CMAKE_CURRENT_SOURCE_DIR}/tests/bodies.cpp
			${CMAKE_CURRENT_SOURCE_DIR}/tests/climate.cpp
			${CMAKE_CURRENT_SOURCE_DIR}/tests/code.cpp
			${CMAKE_CURRENT_SOURCE_DIR}/tests/geology.cpp
			${CMAKE_CURRENT_SOURCE_DIR}/tests/golden.cpp
			${CMAKE_CURRENT_SOURCE_DIR}/tests/ranges.cpp
			${CMAKE_CURRENT_SOURCE_DIR}/tests/soil.cpp
			${CMAKE_CURRENT_SOURCE_DIR}/tests/system.cpp
			${CMAKE_CURRENT_SOURCE_DIR}/tests/water.cpp)
	# where a source build of the game keeps them (.agents/planning/home.md): ~/.terraverse-dev/cache/engine/grids
	if(WIN32)
		# APPDATA has backslashes, which a C string would read as escapes (\U)
		string(REPLACE "\\" "/" TERRAGEN_HOME "$ENV{APPDATA}/Terraverse-dev")
	else()
		set(TERRAGEN_HOME "$ENV{HOME}/.terraverse-dev")
	endif()
	target_compile_definitions(terragen_tests PRIVATE TERRAGEN_TEST_GRIDS="${TERRAGEN_HOME}/cache/engine/grids")
endif()
