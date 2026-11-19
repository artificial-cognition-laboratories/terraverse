add_subdirectory(async)
add_subdirectory(common)
add_subdirectory(emerge)
add_subdirectory(game)

set(BUILTIN_SRCS
	init.lua
	${BUILTIN_ASYNC_SRCS}
	${BUILTIN_COMMON_SRCS}
	${BUILTIN_EMERGE_SRCS}
	${BUILTIN_GAME_SRCS}
	PARENT_SCOPE)

set(BUILTIN_BASE_PATH ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)
