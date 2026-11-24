# far: the far view. `terrain` (the land as blocks) and `horizon` (its
# packet) are built everywhere: the server sends, the unit tests build
# tiles, `palette` and `baked` are read anywhere. `mesh`, `palette_live` (textures),
# `tiles` and `view` are the client's.
set(far_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/land/terrain.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/tiles/tiling.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/land/horizon.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bake/palette.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bake/baked.cpp
	PARENT_SCOPE)

set(far_client_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/tiles/coverage.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bake/export.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bake/landbake.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/tiles/mask.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/land/mesh.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bake/palette_live.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/tiles/tilecache.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/tiles/tiles.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/tiles/workers.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/tiles/budget.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/view.cpp
	PARENT_SCOPE)
