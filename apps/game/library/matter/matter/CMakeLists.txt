# matter: elements and substances, and the states they're in. No engine
# dependencies beyond jsoncpp; the data is matter/*.json at the repo root.
set(matter_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/grains.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/matter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/mixture.cpp
	PARENT_SCOPE)
