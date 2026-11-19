file(GLOB common_server_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

set(common_server_SRCS
	${common_server_HDRS}
	${CMAKE_CURRENT_SOURCE_DIR}/activeobjectmgr.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ban.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/blockmodifier.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/clientiface.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/lakes.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/luaentity_sao.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/mods.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/player_sao.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/rollback.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/serveractiveobject.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/serverinventorymgr.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/serverlist.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/sky.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/gravity.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/planet.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/weather.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/player.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/matter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/unit_sao.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/universe.cpp
	PARENT_SCOPE)
