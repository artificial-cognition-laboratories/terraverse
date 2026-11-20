# planetarium: the planet viewer (planning/sprint-31-planetarium.md). The client's.
set(planetarium_client_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/camera.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/globe/seams.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/overlays/ribbon.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/overlays/paint.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/overlays/markers.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/space/system.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/globe/air.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/globe/globe.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/globe/lit.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/globe/rim.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/space/galaxy.cpp
	${GAME_DIR}/library/space/milkyway/milkyway.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/space/sky.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/planetarium.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/run/standalone.cpp
	PARENT_SCOPE)
