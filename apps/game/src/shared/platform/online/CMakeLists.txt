# online: playing with friends over the internet — STUN, endpoints, the
# ticket gate a hosting server admits joiners through (common), and the
# client's hosting and joining (client).
set(online_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/endpoint.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/gate.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/http.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/stun.cpp
	PARENT_SCOPE)
set(online_client_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/host.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/join.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/players.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/upnp.cpp
	PARENT_SCOPE)
