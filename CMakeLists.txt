cmake_minimum_required (VERSION 2.8)
project(rtsp)
set(CMAKE_BUILD_TYPE "Debug")
# set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb -std=c++11")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -g -std=c++11 -lpthread")
# set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

#SET(CMAKE_C_COMPILER “/usr/local/bin/gcc”)

#SET(CMAKE_CXX_COMPILER “/usr/local/bin/g++”)

aux_source_directory(. SRCS)

add_executable(rtsp ${SRCS})
