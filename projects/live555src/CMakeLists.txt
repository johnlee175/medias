#If build on mac os x, please use commands like:
# mkdir build && cd build && cmake \
# -DCMAKE_C_COMPILER=/usr/local/Cellar/gcc/7.2.0/bin/gcc-7 \
# -DCMAKE_CXX_COMPILER=/usr/local/Cellar/gcc/7.2.0/bin/g++-7 \
# -DCMAKE_C_FLAGS=-fpermissive -DCMAKE_CXX_FLAGS=-fpermissive ../

cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
#set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE Release)

add_definitions(-DOutPacketBufferMaxSize=240000)
add_definitions(-DALLOW_SERVER_PORT_REUSE)

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
message("CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}")

if (APPLE)
    #on mac os x
    message("APPLE")
    message("CMAKE_HOST_APPLE=${CMAKE_HOST_APPLE}")
    add_library(live555 STATIC ${SOURCE_FILES})
elseif(ANDROID)
    #on android
    message("ANDROID")
    find_library(log-lib log)
    add_library(live555 SHARED ${SOURCE_FILES})
    target_link_libraries(live555 ${log-lib})
endif()

add_executable(testOnDemandRTSPServer testProgs/testOnDemandRTSPServer.cpp)
target_link_libraries(testOnDemandRTSPServer live555)
