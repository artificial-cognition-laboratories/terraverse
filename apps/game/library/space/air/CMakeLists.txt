# air: a body's atmosphere as light meets it, from its card, and the tables
# drawing it reads (planning/arc-atmosphere.md). No engine dependencies.
set(air_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/optics.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/tables.cpp
	PARENT_SCOPE)
