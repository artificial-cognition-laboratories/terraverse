# physics: rigid bodies on Jolt Physics (sprint 57). No engine dependencies.
set(physics_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/actor.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/aero.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/driven.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/limbs.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/motion.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/mover.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/pose.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/reach.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/skeleton.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/structure.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/walk.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/world.cpp
	PARENT_SCOPE)
