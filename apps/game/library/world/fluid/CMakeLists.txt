# fluid: a fluid layer on a spinning sphere (weather). Depends on matter for
# what the air is made of and what condenses; nothing else from the engine.
set(fluid_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/circulation.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/events.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/layer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/sample.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/save.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/storms.cpp
	PARENT_SCOPE)
