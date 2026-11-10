# far: the far view. `terrain` (the land as blocks) and `horizon` (its
# packet) are built everywhere: the server sends, the unit tests build
# tiles, `palette` and `baked` are read anywhere. `mesh`, `palette_live` (textures),
# `tiles` and `view` are the client's.
set(far_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/terrain.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/tiling.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/horizon.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/palette.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/baked.cpp
	PARENT_SCOPE)

set(far_client_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/export.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/landbake.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/mesh.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/palette_live.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/tilecache.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/tiles.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/view.cpp
	PARENT_SCOPE)
