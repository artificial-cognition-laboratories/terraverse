file(GLOB_RECURSE common_network_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

set(common_network_SRCS
	${common_network_HDRS}
	${CMAKE_CURRENT_SOURCE_DIR}/address.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/connection.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/mtp/impl.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/mtp/threads.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/networkpacket.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/networkprotocol.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/socket.cpp
	PARENT_SCOPE
)

set(server_network_SRCS
	${GAME_DIR}/src/server/app/network/serveropcodes.cpp
	${GAME_DIR}/src/server/app/network/serverpackethandler.cpp
	PARENT_SCOPE
)

if (BUILD_CLIENT)
	set(client_network_SRCS
		${GAME_DIR}/src/client/world/network/clientopcodes.cpp
		${GAME_DIR}/src/client/world/network/clientpackethandler.cpp
		PARENT_SCOPE
	)
endif()
