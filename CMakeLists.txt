cmake_minimum_required (VERSION 2.8)
project(rtsp)
#set(CMAKE_BUILD_TYPE "Release")
set(CMAKE_BUILD_TYPE "Debug")

#set(CMAKE_CXX_FLAGS "-Wdeprecated-declaraions")

set(CMAKE_EXE_LINKER_FLAGS "-lpthread -lz -lva -lva-drm")
#set(CMAKE_EXE_LINKER_FLAGS_DEBUG  "-lavcodec -lavformat -lavdevice -lavutil -lavfilter  -lpthread")

set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")

set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")


#set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb -std=c++11")
#set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -g  -lavcodec -lavformat -lavdevice -lavutil -lavfilter   -lswresample -lswscale -std=c++11 -lpthread -lz -lm -ldl -lX11")
#set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -g  -lavformat  -lavdevice -lavcodec  -lavutil -std=c++11 -lpthread  ")
#set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -g -lavformat -std=c++11 -lpthread  ")

# set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

#SET(CMAKE_C_COMPILER “/usr/local/bin/gcc”)

#SET(CMAKE_CXX_COMPILER “/usr/local/bin/g++”)

aux_source_directory(. SRCS)

include_directories("/usr/local/ffmpeg/include")
link_directories(/usr/local/ffmpeg/lib)
link_libraries(libavformat.a libavdevice.a libavcodec.a libavfilter.a libavutil.a libswresample.a libswscale.a libX11.so ) 


add_executable(rtsp ${SRCS})
