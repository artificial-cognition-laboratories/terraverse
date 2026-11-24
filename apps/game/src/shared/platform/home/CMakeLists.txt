# home: where Terraverse keeps things on this machine (.agents/planning/home.md): the root,
# its profiles and its cache, and moving an older layout in.
set(home_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/home.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/browser.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/moving.cpp
	PARENT_SCOPE)
