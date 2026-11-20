# release: what a build knows about releases: versions, signed manifests, and
# which version last saved a world (common: a headless server has worlds too).
set(release_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/release.cpp
	PARENT_SCOPE)
