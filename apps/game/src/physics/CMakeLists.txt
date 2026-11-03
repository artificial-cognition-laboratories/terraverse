# physics: rigid bodies on Jolt Physics (sprint 57). No engine dependencies.
set(physics_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/pose.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/reach.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/skeleton.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/walk.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/world.cpp
	PARENT_SCOPE)
