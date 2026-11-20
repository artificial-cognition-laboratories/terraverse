# Jolt Physics (MIT, https://github.com/jrouwe/JoltPhysics), vendored at v5.4.0:
# only its Jolt/ sources and LICENSE. Built through its own Jolt.cmake with the
# options Terra Firma needs (planning/sprint-57-rolling.md):
# - double precision: positions stay exact far from the origin
# - cross-platform determinism: the same inputs, the same results everywhere
# - SSE4.2 and no newer: the Linux package runs on any x86-64 of the last decade
set(PHYSICS_REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(DOUBLE_PRECISION ON)
set(CROSS_PLATFORM_DETERMINISTIC ON)
set(USE_SSE4_1 ON)
set(USE_SSE4_2 ON)
set(USE_AVX OFF)
set(USE_AVX2 OFF)
set(USE_AVX512 OFF)
set(USE_LZCNT OFF)
set(USE_TZCNT OFF)
set(USE_F16C OFF)
set(USE_FMADD OFF)
set(INTERPROCEDURAL_OPTIMIZATION OFF)
set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF)
set(OBJECT_LAYER_BITS 16)
set(ENABLE_OBJECT_STREAM OFF)
set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF)
set(PROFILER_IN_DEBUG_AND_RELEASE OFF)
set(GENERATE_DEBUG_SYMBOLS OFF)
set(ENABLE_INSTALL OFF)
include(${PHYSICS_REPO_ROOT}/Jolt/Jolt.cmake)
# It's ours to warn about, not theirs
set_target_properties(Jolt PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(Jolt PRIVATE -w)
