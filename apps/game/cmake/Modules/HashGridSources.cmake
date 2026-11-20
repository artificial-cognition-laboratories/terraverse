# One sha256 of a list of sources: what kept grids are named by (the sources
# terragen's grids are built from, terragen/CMakeLists.txt), and kept far
# tiles (what a tile's built from: far/CMakeLists.txt). TEMPLATE and OUTPUT
# say which; the template reads @SOURCES_SHA256@.

file(READ "${GRID_SRCS_FILE}" GRID_SRCS)
set(ALL "")
foreach(P ${GRID_SRCS})
	file(SHA256 ${P} H)
	string(APPEND ALL "${H}")
endforeach()
string(SHA256 SOURCES_SHA256 "${ALL}")

# (written only if it changed: the hash holds when a column's read changes)
configure_file("${TEMPLATE}" "${OUTPUT}")
