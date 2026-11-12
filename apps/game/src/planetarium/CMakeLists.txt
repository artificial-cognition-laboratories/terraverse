# planetarium: the planet viewer (planning/sprint-31-planetarium.md). The client's.
set(planetarium_client_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/camera.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/seams.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ribbon.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/paint.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/markers.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/system.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/air.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/globe.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/lit.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/rim.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/galaxy.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../milkyway/milkyway.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/sky.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/planetarium.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/standalone.cpp
	PARENT_SCOPE)
