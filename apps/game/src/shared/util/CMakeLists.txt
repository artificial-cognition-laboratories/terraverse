file(GLOB util_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*/*.h")

set(util_SRCS
	${util_HDRS}
	${CMAKE_CURRENT_SOURCE_DIR}/math/areastore.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/text/base64.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/text/colorize.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/math/directiontables.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/text/enriched_string.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/hash/guid.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/hash/hashing.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/math/ieee_float.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/timing/metricsbackend.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/math/half.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/math/numeric.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../game/pointing/pointedthing.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../game/pointing/pointabilities.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/timing/quicktune.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/data/serialize.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/image/screenshot.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/hash/sha1.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/text/string.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/timing/timetaker.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/image/png.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/text/enum_string.cpp
	PARENT_SCOPE)
