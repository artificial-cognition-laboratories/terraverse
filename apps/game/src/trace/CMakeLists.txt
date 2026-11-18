# trace: the always-on zone recorder behind the debugger's Perf tab. Client
# and server both record; no engine dependencies.
set(trace_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/net.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/trace.cpp
	PARENT_SCOPE)
