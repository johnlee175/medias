cmake_minimum_required(VERSION 3.8)
project(live555)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_C_COMPILER /usr/local/Cellar/gcc/7.2.0/bin/gcc-7)
set(CMAKE_CXX_COMPILER /usr/local/Cellar/gcc/7.2.0/bin/g++-7)

set(CMAKE_CXX_FLAGS -fpermissive)

include_directories(
        UsageEnvironment/include
        BasicUsageEnvironment/include
        groupsock/include
        liveMedia/include
        liveMedia
)

file(GLOB_RECURSE UsageEnvironmentSrc UsageEnvironment/*.c UsageEnvironment/*.cpp)
file(GLOB_RECURSE BasicUsageEnvironmentSrc BasicUsageEnvironment/*.c BasicUsageEnvironment/*.cpp)
file(GLOB_RECURSE groupsockSrc groupsock/*.c groupsock/*.cpp)
file(GLOB_RECURSE liveMediaSrc liveMedia/*.c liveMedia/*.cpp)

set(SOURCE_FILES
        ${UsageEnvironmentSrc}
        ${BasicUsageEnvironmentSrc}
        ${groupsockSrc}
        ${liveMediaSrc}
)
add_library(live555 STATIC ${SOURCE_FILES})


add_executable(testOnDemandRTSPServer testProgs/testOnDemandRTSPServer.cpp)
target_link_libraries(testOnDemandRTSPServer live555)
