cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

add_definitions(-DOutPacketBufferMaxSize=240000)
add_definitions(-DALLOW_SERVER_PORT_REUSE)
add_definitions(-DCMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
#add_definitions(-DJOHN_DEBUG)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror \
    -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-comment")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror \
    -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-comment")

option(APPLY_TRACE "Trace compile and malloc/free" OFF)
if (APPLY_TRACE)
    add_definitions(-DTRACE_MALLOC -DTRACE_FREE)
    set(CMAKE_VERBOSE_MAKEFILE  ON)
endif ()

if (ANDROID)
    include(Android.cmake)
else()
    include(Macosx.cmake)
endif ()
