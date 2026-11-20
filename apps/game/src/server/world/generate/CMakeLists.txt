file(GLOB mapgen_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/*/*.h")

set(mapgen_SRCS
	${mapgen_HDRS}
	${CMAKE_CURRENT_SOURCE_DIR}/mapgen.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/luanti/mapgen_singlenode.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/mapgen_terra.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/land/terra_column.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/land/atlas_land.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/place/terra_species.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/place/terra_flora.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/luanti/mg_biome.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/luanti/mg_decoration.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/luanti/mg_ore.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/luanti/mg_schematic.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/luanti/treegen.cpp
	PARENT_SCOPE
)
