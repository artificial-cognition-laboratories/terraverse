# orbit: flight on a planet (terragen's Planet card): gravity, air, thrust,
# the orbit a state is on; the field (sprint 59): that pull in the atlas's
# grids. No engine dependencies.
set(orbit_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/field.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/flight.cpp
	PARENT_SCOPE)
