# terragen: the world model. No engine dependencies — the map generator, the
# menu preview and the unit tests all call it.
set(terragen_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/atlas.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/body.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/calendar.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/climate.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/craters.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/code.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/fixed.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/geology.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/grid.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/grid_file.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/history.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/maria.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/lid.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/channels.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/noise.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/parallel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/planet.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/preview.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/progress.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/soil.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/sphere.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/sun.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/system.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/vegetation.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/world.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/worlds.cpp)
set(terragen_SRCS ${terragen_SRCS} PARENT_SCOPE) # (and here: the grid's hash below reads it)

# What a grid is built from: every source but those only a column's read runs
# (the drainage land, soil, cover, the preview, the atlas). A kept grid is
# named by their hash, so a change to how columns are read keeps the planets
# already worked out (grid_file.cpp); anything else here works them out again.
set(terragen_sampling channels.cpp soil.cpp vegetation.cpp preview.cpp atlas.cpp progress.cpp)
file(GLOB terragen_headers "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
set(terragen_grid_SRCS ${terragen_headers})
foreach(src ${terragen_SRCS})
	get_filename_component(name ${src} NAME)
	if(NOT name IN_LIST terragen_sampling)
		list(APPEND terragen_grid_SRCS ${src})
	endif()
endforeach()
set(terragen_grid_SRCS ${terragen_grid_SRCS} PARENT_SCOPE)
