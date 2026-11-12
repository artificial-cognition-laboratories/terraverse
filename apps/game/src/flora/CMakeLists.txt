# flora: plants grown from where they stand. No engine dependencies, as
# terragen (whose fixed point it's written in): the wiki and the unit tests
# call it, and the generator will.
set(flora_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/forest.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/mat.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/site.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/plant.cpp
	PARENT_SCOPE)
