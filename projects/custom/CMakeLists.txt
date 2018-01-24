cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_BUILD_TYPE Debug)

add_definitions(-DOutPacketBufferMaxSize=240000)
add_definitions(-DALLOW_SERVER_PORT_REUSE)
add_definitions(-DCMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror \
    -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-comment")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror \
    -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-comment")

option(APPLY_TRACE "Trace/Debug/Verbose" OFF)
if (APPLY_TRACE)
    add_definitions(-DTRACE_MALLOC -DTRACE_FREE)
    add_definitions(-DJOHN_DEBUG)
    set(CMAKE_VERBOSE_MAKEFILE ON)
    set(CMAKE_BUILD_TYPE Debug)
endif ()

if (ANDROID)
    set(PLATFORM_SUBDIR android)
    include(Android.cmake)
else()
    if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -lpthread")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lpthread")

        set(PLATFORM_SUBDIR linux)
        include(Linux.cmake)
    else()
        set(PLATFORM_SUBDIR macosx)
        include(Macosx.cmake)
    endif ()
endif ()
