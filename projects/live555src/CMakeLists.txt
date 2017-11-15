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

#on mac os x
#add_library(live555 STATIC ${SOURCE_FILES})
#on android
add_library(live555 SHARED ${SOURCE_FILES})

add_executable(testOnDemandRTSPServer testProgs/testOnDemandRTSPServer.cpp)
target_link_libraries(testOnDemandRTSPServer live555)
