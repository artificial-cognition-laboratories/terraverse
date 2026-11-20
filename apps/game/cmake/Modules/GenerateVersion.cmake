# Always run during 'make'

# Terra Firma: a release is its tag (-DTERRAFIRMA_VERSION=v0.2.0). Joining a
# friend compares this (src/online/gate.h), so every release must differ.
if(TERRAFIRMA_VERSION)
	set(VERSION_GITHASH "${TERRAFIRMA_VERSION}")
endif()

if(DEVELOPMENT_BUILD AND NOT VERSION_GITHASH)
	execute_process(COMMAND git rev-parse --short HEAD
		WORKING_DIRECTORY "${GENERATE_VERSION_SOURCE_DIR}"
		OUTPUT_VARIABLE VERSION_GITHASH OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET)
	if(VERSION_GITHASH)
		set(VERSION_GITHASH "${VERSION_STRING}-${VERSION_GITHASH}")
		execute_process(COMMAND git diff-index --quiet HEAD
			WORKING_DIRECTORY "${GENERATE_VERSION_SOURCE_DIR}"
			RESULT_VARIABLE IS_DIRTY)
		if(IS_DIRTY)
			set(VERSION_GITHASH "${VERSION_GITHASH}-dirty")
		endif()
		message(STATUS "*** Detected Git version ${VERSION_GITHASH} ***")
	endif()
endif()
if(NOT VERSION_GITHASH)
	if(DEVELOPMENT_BUILD)
		set(VERSION_GITHASH "${VERSION_STRING}-unknown")
	else()
		set(VERSION_GITHASH "${VERSION_STRING}")
	endif()
endif()

configure_file(
	${GENERATE_VERSION_SOURCE_DIR}/cmake_config_githash.h.in
	${GENERATE_VERSION_BINARY_DIR}/cmake_config_githash.h)

