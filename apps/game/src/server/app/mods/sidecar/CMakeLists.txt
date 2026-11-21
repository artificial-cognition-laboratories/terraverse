# sidecar: TypeScript mods, run by `terra runtime` in a process of their own (sprint 119).
# The engine opens a window each step and answers the sidecar's calls inside it.
set(sidecar_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/msgpack.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/sidecar.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/terra.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ledger.cpp
	PARENT_SCOPE)
